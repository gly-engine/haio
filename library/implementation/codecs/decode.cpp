#include <haio/generated/codecs.hpp>

#include <haio/implementation/registry.hpp>
#include <haio/util/reflection.hpp>

#include <meta>
#include <string>

namespace Haio::Implementation::Codecs {

Result<AnyImage> Decode<>::operator()(CodecIn blob, const Settings& settings) const {
    const auto found = blob.format != Format::RAW ? Found{blob.format, blob.color} : Detect<>{}(blob.data);
    if (!found) {
        return std::unexpected(Error{ErrorCode::UnsupportedFormat, "unrecognised input"});
    }

    Result<AnyImage> out =
        std::unexpected(Error{ErrorCode::UnsupportedFormat,
                              "cannot decode " + std::string(formatName(found.format))
                                  + " " + std::string(colorName(found.color))});

    HAIO_FOR_EACH_FORMAT(f) {
        constexpr Format format = std::meta::extract<Format>(f);
        HAIO_FOR_EACH_COLOR(c) {
            constexpr Color color = std::meta::extract<Color>(c);
            if constexpr (Decodable<format, color>) {
                if (format != found.format || color != found.color) continue;

                auto decoded = decode<format, color>(blob, settings);
                if (!decoded) {
                    out = std::unexpected(decoded.error());
                } else if constexpr (color != Color::PALETTE) {
                    out = AnyImage{color, decoded->width, decoded->height, std::move(decoded->data)};
                } else {
                    auto expanded = Convert<Color::PALETTE, Color::RGBA8888>{}(*std::move(decoded));
                    if (!expanded) {
                        out = std::unexpected(expanded.error());
                    } else {
                        out = AnyImage{Color::RGBA8888, expanded->width, expanded->height, std::move(expanded->data)};
                    }
                }
            }
        }
    }
    return out;
}

}

namespace Haio::Implementation {

Result<Image<Color::RGBA8888>> decode(const Blob& blob) {
    HAIO_TRY(native, Codecs::Decode<>{}(blob));
    HAIO_TRY(rgba, Codecs::Convert<>{}(std::move(native), Color::RGBA8888));
    return Image<Color::RGBA8888>{rgba.width, rgba.height, std::move(rgba.data)};
}

}

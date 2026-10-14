#include <haio/generated/codecs.hpp>

#include <haio/implementation/registry.hpp>
#include <haio/util/reflection.hpp>

#include <meta>
#include <string>

namespace Haio::Implementation::Codecs {

/** @cond */
Result<Blob> Encode<>::operator()(AnyImage image, Format to, std::optional<Color> as, const Settings& settings) const {
    const auto wanted = as ? as : encodeColorFor(to);
    if (!wanted) {
        return std::unexpected(Error{ErrorCode::UnsupportedFormat,
                                     "cannot encode " + std::string(formatName(to))});
    }

    Result<Blob> out = std::unexpected(Error{ErrorCode::UnsupportedFormat,
                                             "a " + std::string(formatName(to)) + " cannot hold "
                                                 + std::string(colorName(*wanted))});
    bool done = false;

    HAIO_FOR_EACH_COLOR(c) {
        constexpr Color color = std::meta::extract<Color>(c);
        HAIO_FOR_EACH_FORMAT(f) {
            constexpr Format format = std::meta::extract<Format>(f);
            if constexpr (Encodable<format, color> && color != Color::PALETTE) {
                if (done || format != to || color != *wanted) continue;
                done = true;

                // the pair exists and the road to it may not, which is a different
                // sentence from the container not holding that colour at all
                auto converted = Convert<>{}(std::move(image), color);
                out = converted ? encode<format, color>(
                                      Image<color>{converted->width, converted->height, std::move(converted->data)},
                                      settings)
                                : Result<Blob>{std::unexpected(converted.error())};
            }
        }
    }
    return out;
}

Result<Blob> Encode<>::operator()(Image<Color::RGBA8888> image, Format to, std::optional<Color> as) const {
    return (*this)(AnyImage{Color::RGBA8888, image.width, image.height, std::move(image.data)}, to, as);
}

Result<Blob> Encode<>::operator()(Image<Color::PALETTE> image, Format to, const Settings& settings) const {
    Result<Blob> out = std::unexpected(Error{ErrorCode::UnsupportedFormat,
                                             "a " + std::string(formatName(to)) + " cannot hold a palette"});
    HAIO_FOR_EACH_FORMAT(f) {
        constexpr Format format = std::meta::extract<Format>(f);
        if constexpr (Encodable<format, Color::PALETTE>) {
            if (format != to) continue;
            out = encode<format, Color::PALETTE>(std::move(image), settings);
        }
    }
    return out;
}

/** @endcond */

}

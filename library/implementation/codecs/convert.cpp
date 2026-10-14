#include <haio/generated/codecs.hpp>

#include <haio/implementation/registry.hpp>
#include <haio/util/reflection.hpp>

#include <meta>
#include <string>

namespace Haio::Implementation::Codecs {

/** @cond */
Result<AnyImage> Convert<>::operator()(AnyImage image, Color to) const {
    if (image.color == to) return image;

    const auto from = image.color;
    Result<AnyImage> out =
        std::unexpected(Error{ErrorCode::UnsupportedFormat,
                              "no route from " + std::string(colorName(from)) + " to " + std::string(colorName(to))});
    bool done = false;

    HAIO_FOR_EACH_COLOR(f) {
        constexpr Color source = std::meta::extract<Color>(f);
        if constexpr (source != Color::PALETTE) {
            if (done || source != from) continue;

            HAIO_FOR_EACH_COLOR(t) {
                constexpr Color target = std::meta::extract<Color>(t);
                if constexpr (Convertible<source, target>) {
                    if (target != to) continue;

                    auto converted = Convert<source, target>{}(
                        Image<source>{image.width, image.height, std::move(image.data)});
                    if (!converted) {
                        out = std::unexpected(converted.error());
                    } else {
                        out = AnyImage{target, converted->width, converted->height, std::move(converted->data)};
                    }
                    done = true;
                }
            }
        }
    }

    if (done || from == Color::RGBA8888 || to == Color::RGBA8888) return out;

    HAIO_TRY(middle, (*this)(std::move(image), Color::RGBA8888));
    return (*this)(std::move(middle), to);
}

Result<std::vector<uint8_t>> Convert<>::operator()(const Image<Color::RGBA8888>& image, Color to) const {
    HAIO_TRY(converted, (*this)(AnyImage{Color::RGBA8888, image.width, image.height, image.data}, to));
    return std::move(converted.data);
}

/** @endcond */

}

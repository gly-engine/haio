#include <haio/generated/codecs.hpp>

#include <haio/implementation/registry.hpp>
#include <haio/util/reflection.hpp>

#include <meta>
#include <string>

namespace Haio::Implementation::Codecs {

/** @cond */
Result<AnyImage> Generate<>::operator()(Brush brush, std::string_view words, const Settings& settings) const {
    Result<AnyImage> out = std::unexpected(Error{ErrorCode::UnsupportedFormat,
                                                 "this build cannot paint " + std::string(brushName(brush))});
    bool done = false;
    HAIO_FOR_EACH_BRUSH(b) {
        constexpr Brush painter = std::meta::extract<Brush>(b);
        HAIO_FOR_EACH_COLOR(c) {
            constexpr Color color = std::meta::extract<Color>(c);
            if constexpr (Generatable<painter, color> && color != Color::PALETTE) {
                if (done || painter != brush) continue;
                done = true;
                auto painted = Generate<painter, color>{}(words, settings);
                out = painted ? Result<AnyImage>{AnyImage{color, painted->width, painted->height, std::move(painted->data)}}
                              : Result<AnyImage>{std::unexpected(painted.error())};
            }
        }
    }
    return out;
}

/** @endcond */

}

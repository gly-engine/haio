#include <haio/generated/transforms.hpp>

#include <haio/implementation/transforms/composite.hpp>

#include <meta>
#include <string_view>

namespace Haio::Implementation::Transforms {

/** @cond */
// every operator asked in turn, the way the command line names one
BlendRes Blend<>::operator()(Compose compose, BlendDst<> dst, BlendSrc src, int x, int y) const {
    std::string_view named;
    template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^Compose))) {
        constexpr Compose mode = std::meta::extract<Compose>(e);
        if constexpr (Blendable<mode, Color::RGBA8888, Color::RGBA8888>) {
            if (mode == compose) return Blend<mode, Color::RGBA8888, Color::RGBA8888>{}(std::move(dst), src, x, y);
        }
        if (mode == compose) named = std::meta::identifier_of(e);
    }
    HAIO_FAIL(UnsupportedFormat, "this build cannot -compose " + Util::quoted(named));
}

/** @endcond */

}

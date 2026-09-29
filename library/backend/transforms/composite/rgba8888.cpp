#include <haio/transforms/composite.hpp>
#include <haio/generated/transform.hpp>

#include <meta>
#include <string_view>

namespace Haio::Transforms {

namespace {

Result<Image<Color::RGBA8888>> blend(Image<Color::RGBA8888> base, const Image<Color::RGBA8888>& layer, Compose compose,
                                     int x, int y) {
    std::string_view named;
    template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^Compose))) {
        constexpr Compose mode = std::meta::extract<Compose>(e);
        if constexpr (Blendable<mode>) {
            if (mode == compose) return Blend<mode>(std::move(base), layer, x, y);
        }
        if (mode == compose) named = std::meta::identifier_of(e);
    }
    HAIO_FAIL(UnsupportedFormat, "this build cannot -compose " + Stages::quoted(named));
}

}

template <>
Result<Image<Color::RGBA8888>> Composite<Color::RGBA8888>(Image<Color::RGBA8888> base, const Image<Color::RGBA8888>& layer, Compose compose, Gravity gravity, int x, int y) {
    const auto [left, top] = placeOf({base.width, base.height}, {layer.width, layer.height}, gravity, x, y);
    return blend(std::move(base), layer, compose, left, top);
}

}

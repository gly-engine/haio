#pragma once

#include "haio_transform.hpp"
#include "haio/stage.hpp"

#include <algorithm>
#include <utility>

namespace Haio {

/**
 * which edge or corner a layer is measured from, named the way imagemagick's
 * -gravity names them; the names on the command line are these, lowered.
 */
enum class Gravity {
    NorthWest,
    North,
    NorthEast,
    West,
    Center,
    East,
    SouthWest,
    South,
    SouthEast,

    /** imagemagick's two ways of saying there is no gravity, which measure from the top left */
    None,
    Forget,
};

/**
 * where a layer's top left corner lands, the way imagemagick works it out: the
 * offset is measured away from the edge the gravity names, so +10 under an east
 * gravity moves the layer left, and a centred axis adds it to the middle.
 */
constexpr std::pair<int, int> placeOf(Size base, Size layer, Gravity gravity, int dx, int dy) {
    const auto column = [&](int left, int middle, int right) {
        switch (gravity) {
            case Gravity::NorthWest: case Gravity::West: case Gravity::SouthWest:
            case Gravity::None: case Gravity::Forget: return left;
            case Gravity::North: case Gravity::Center: case Gravity::South: return middle;
            case Gravity::NorthEast: case Gravity::East: case Gravity::SouthEast: return right;
        }
        return left;
    };
    const auto row = [&](int top, int middle, int bottom) {
        switch (gravity) {
            case Gravity::NorthWest: case Gravity::North: case Gravity::NorthEast:
            case Gravity::None: case Gravity::Forget: return top;
            case Gravity::West: case Gravity::Center: case Gravity::East: return middle;
            case Gravity::SouthWest: case Gravity::South: case Gravity::SouthEast: return bottom;
        }
        return top;
    };
    return {column(dx, (base.width - layer.width) / 2 + dx, base.width - layer.width - dx),
            row(dy, (base.height - layer.height) / 2 + dy, base.height - layer.height - dy)};
}

/**
 * how -composite mixes src into the dst under it. every formula is over straight
 * colours from 0 to 1.
 */
enum class Compose {
    /** @ref Haio::Transforms::Blend<Compose::Over> */
    Over,
    /** @ref Haio::Transforms::Blend<Compose::Multiply> */
    Multiply,
    /** @ref Haio::Transforms::Blend<Compose::Screen> */
    Screen,
    /** @ref Haio::Transforms::Blend<Compose::Overlay> */
    Overlay,
    /** @ref Haio::Transforms::Blend<Compose::Darken> */
    Darken,
    /** @ref Haio::Transforms::Blend<Compose::Lighten> */
    Lighten,
    /** @ref Haio::Transforms::Blend<Compose::ColorDodge> */
    ColorDodge,
    /** @ref Haio::Transforms::Blend<Compose::ColorBurn> */
    ColorBurn,
    /** @ref Haio::Transforms::Blend<Compose::HardLight> */
    HardLight,
    /** @ref Haio::Transforms::Blend<Compose::SoftLight> */
    SoftLight,
    /** @ref Haio::Transforms::Blend<Compose::Difference> */
    Difference,
    /** @ref Haio::Transforms::Blend<Compose::Exclusion> */
    Exclusion,
    /** @ref Haio::Transforms::Blend<Compose::Plus> */
    Plus,
    /** @ref Haio::Transforms::Blend<Compose::DstIn> */
    DstIn,
    /**
     * @ref Haio::Transforms::Blend<Compose::Tint> @n
     * @ref Haio::Transforms::Blend<Compose::Tint, Color::GRAYALPHA88> @n
     * @ref Haio::Transforms::Blend<Compose::Tint, Color::GRAY8>
     */
    Tint,
};

/**
 * straight alpha in, worked out premultiplied as imagemagick 6 does: colour(Sc, Sa, Dc, Da)
 * is the premultiplied channel and alpha(Sa, Da) the coverage, both from 0 to 1.
 */
template <typename Colour, typename Alpha>
Image<Color::RGBA8888> blendWith(Image<Color::RGBA8888> base, const Image<Color::RGBA8888>& layer, int x, int y,
                                 Colour&& colour, Alpha&& alpha) {
    const int x0 = std::max(0, x);
    const int y0 = std::max(0, y);
    const int x1 = std::min(base.width, x + layer.width);
    const int y1 = std::min(base.height, y + layer.height);

    // through imagemagick's sixteen bit quantum; it keeps transparency, not alpha, so that is what is cut
    const auto byteOf = [](double share) {
        return static_cast<uint8_t>(static_cast<uint32_t>(std::clamp(share * 65535.0 + 0.5, 0.0, 65535.0)) / 257);
    };

    for (int by = y0; by < y1; by++) {
        for (int bx = x0; bx < x1; bx++) {
            auto* under = base.data.data() + (static_cast<size_t>(by) * static_cast<size_t>(base.width) + static_cast<size_t>(bx)) * 4;
            const auto* over = layer.data.data()
                             + (static_cast<size_t>(by - y) * static_cast<size_t>(layer.width) + static_cast<size_t>(bx - x)) * 4;
            if (over[3] == 0) continue;

            const double sa = over[3] / 255.0;
            const double da = under[3] / 255.0;
            const double ra = std::clamp(alpha(sa, da), 0.0, 1.0);
            for (int c = 0; c < 3; c++) {
                const double premultiplied = colour(over[c] / 255.0, sa, under[c] / 255.0, da);
                under[c] = ra > 0 ? byteOf(premultiplied / ra) : 0;
            }
            under[3] = static_cast<uint8_t>(255 - byteOf(1 - ra));
        }
    }
    return base;
}

/** the layer's pixel, or opaque black where it does not reach, which tints nothing */
inline const uint8_t* colourAt(const Image<Color::RGBA8888>& layer, int x, int y) {
    static constexpr uint8_t black[4] = {0, 0, 0, 0xFF};
    if (x < 0 || y < 0 || x >= layer.width || y >= layer.height) return black;
    return layer.data.data() + (static_cast<size_t>(y) * static_cast<size_t>(layer.width) + static_cast<size_t>(x)) * 4;
}

/** a times b, both out of 255, rounded */
constexpr uint8_t timesOf(int a, int b) {
    return static_cast<uint8_t>((a * b + 127) / 255);
}

/** inverted, multiplied, inverted back */
constexpr uint8_t screenOf(int a, int b) {
    return static_cast<uint8_t>(255 - timesOf(255 - a, 255 - b));
}


constexpr double unionOf(double sa, double da) {
    return sa + da - sa * da;
}

/**
 * a w3c separable blend, B(dst, src) where both are opaque, and around it:
 *
 * @startuml{math}
 * {: ("result"_"rgb" = ("src"_"rgb" "src"_"a" (1 - "dst"_"a") + "dst"_"rgb" "dst"_"a" (1 - "src"_"a") + "dst"_"a" "src"_"a" B("dst"_"rgb", "src"_"rgb")) / "result"_"a"), ("result"_"a" = "dst"_"a" + "src"_"a"(1 - "dst"_"a")) :}
 * @enduml
 */
template <typename B>
Image<Color::RGBA8888> blendSeparable(Image<Color::RGBA8888> base, const Image<Color::RGBA8888>& layer, int x, int y,
                                      B&& blend) {
    return blendWith(std::move(base), layer, x, y,
                     [&](double sc, double sa, double dc, double da) {
                         return sc * sa * (1 - da) + dc * da * (1 - sa) + sa * da * blend(sc, dc);
                     },
                     unionOf);
}

namespace Transforms {

/**
 * one per colour the base can be, in `library/backend/transforms/composite/COLOUR.cpp`.
 * the layer is always rgba8888: it has come out of its own pipeline, and it is the
 * layer's alpha that says where it covers.
 */
template <Color P>
Result<Image<P>> Composite(Image<P> base, const Image<Color::RGBA8888>& layer, Compose compose, Gravity gravity,
                           int x, int y) = delete;

template <Color P>
concept Composable = requires (Image<P> b, const Image<Color::RGBA8888>& l, Compose c, Gravity g, int x) {
    { Composite<P>(std::move(b), l, c, g, x, x) } -> std::same_as<Result<Image<P>>>;
};

/** @cond */
template <Compose C, Color Base = Color::RGBA8888, Color Layer = Color::RGBA8888>
Result<Image<Color::RGBA8888>> Blend(Image<Base> base, const Image<Layer>& layer, int x, int y) = delete;
/** @endcond */

template <Compose C, Color Base = Color::RGBA8888, Color Layer = Color::RGBA8888>
concept Blendable = requires (Image<Base> b, const Image<Layer>& l, int x) {
    { Blend<C, Base, Layer>(std::move(b), l, x, x) } -> std::same_as<Result<Image<Color::RGBA8888>>>;
};

}

namespace Stages {

inline constexpr Option compositeOptions[] = {
    {.spellings = {"geometry"}, .takes = "offset", .help = "how far the layer is moved from where gravity puts it, as +X+Y",
     .fallback = "+0+0"},
    {.spellings = {"gravity"}, .takes = "name", .help = "which edge or corner the offset is measured from",
     .shape = Shape::Name, .names = namesOf<Gravity>(), .fallback = "northwest", .called = "gravity type"},
    {.spellings = {"compose"}, .takes = "name",
     .help = "how the layer and the picture under it are mixed",
     .shape = Shape::Name, .names = namesOf<Compose>(), .fallback = "over", .called = "compose operator"},
};

/** "dst src -composite" lays src over dst, and "dst src1 src2" src1 tinted by src2 */
inline constexpr Stage composite{
    .spellings = {"composite"},
    .rule = "composite",
    .options = compositeOptions,
    .help = "lay the picture after the first over it, or the second tinted by the third",
    .merges = true,
};

template <> Built build<&composite>(std::string_view value, const Given& given);

}

}

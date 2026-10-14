#pragma once

#include "haio/implementation/wip/dither.hpp"
#include "haio/implementation/transforms.hpp"
#include "haio/util/family.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace Haio::Implementation {

/**
 * where a layer's top left corner lands, the way imagemagick works it out: the
 * offset is measured away from the edge the gravity names, so +10 under an east
 * gravity moves the layer left, and a centred axis adds it to the middle.
 */
constexpr std::pair<int, int> placeOf(Size base, Size layer, Transforms::Transform<>::Gravity gravity, int dx, int dy) {
    using enum Transforms::Transform<>::Gravity;
    const auto column = [&](int left, int middle, int right) {
        switch (gravity) {
            case NorthWest: case West: case SouthWest:
            case None: case Forget: return left;
            case North: case Center: case South: return middle;
            case NorthEast: case East: case SouthEast: return right;
        }
        return left;
    };
    const auto row = [&](int top, int middle, int bottom) {
        switch (gravity) {
            case NorthWest: case North: case NorthEast:
            case None: case Forget: return top;
            case West: case Center: case East: return middle;
            case SouthWest: case South: case SouthEast: return bottom;
        }
        return top;
    };
    return {column(dx, (base.width - layer.width) / 2 + dx, base.width - layer.width - dx),
            row(dy, (base.height - layer.height) / 2 + dy, base.height - layer.height - dy)};
}

/** a share from 0 to 1 through imagemagick's sixteen bit quantum and down to a byte */
constexpr uint8_t quantumByteOf(double share) {
    return static_cast<uint8_t>(static_cast<uint32_t>(std::clamp(share * 65535.0 + 0.5, 0.0, 65535.0)) / 257);
}

/**
 * straight alpha in, worked out premultiplied as imagemagick 6 does: colour(Sc, Sa, Dc, Da)
 * is the premultiplied channel and alpha(Sa, Da) the coverage, both from 0 to 1.
 */
template <typename Colour, typename Alpha>
Image<Color::RGBA8888> blendWith(Image<Color::RGBA8888> dst, const Image<Color::RGBA8888>& src, int x, int y,
                                 Colour&& colour, Alpha&& alpha) {
    const int x0 = std::max(0, x);
    const int y0 = std::max(0, y);
    const int x1 = std::min(dst.width, x + src.width);
    const int y1 = std::min(dst.height, y + src.height);

    for (int by = y0; by < y1; by++) {
        for (int bx = x0; bx < x1; bx++) {
            auto* under = dst.data.data() + (static_cast<size_t>(by) * static_cast<size_t>(dst.width) + static_cast<size_t>(bx)) * 4;
            const auto* over = src.data.data()
                             + (static_cast<size_t>(by - y) * static_cast<size_t>(src.width) + static_cast<size_t>(bx - x)) * 4;
            if (over[3] == 0) continue;

            const double sa = over[3] / 255.0;
            const double da = under[3] / 255.0;
            const double ra = std::clamp(alpha(sa, da), 0.0, 1.0);
            for (int c = 0; c < 3; c++) {
                const double premultiplied = colour(over[c] / 255.0, sa, under[c] / 255.0, da);
                under[c] = ra > 0 ? quantumByteOf(premultiplied / ra) : 0;
            }
            // imagemagick keeps transparency, not alpha, so that is what is cut
            under[3] = static_cast<uint8_t>(255 - quantumByteOf(1 - ra));
        }
    }
    return dst;
}

/** the src's pixel, or opaque black where it does not reach */
inline const uint8_t* colourAt(const Image<Color::RGBA8888>& src, int x, int y) {
    static constexpr uint8_t black[4] = {0, 0, 0, 0xFF};
    if (x < 0 || y < 0 || x >= src.width || y >= src.height) return black;
    return src.data.data() + (static_cast<size_t>(y) * static_cast<size_t>(src.width) + static_cast<size_t>(x)) * 4;
}

/** a times b, both out of 255, rounded */
constexpr uint8_t timesOf(int a, int b) {
    return static_cast<uint8_t>((a * b + 127) / 255);
}

constexpr double unionOf(double sa, double da) {
    return sa + da - sa * da;
}

/**
 * a w3c separable blend, B(dst, src) where both are opaque, and around it:
 *
 * @startuml{math}
 * {: ("res"_"rgb" = ("src"_"rgb" "src"_"a" (1 - "dst"_"a") + "dst"_"rgb" "dst"_"a" (1 - "src"_"a") + "dst"_"a" "src"_"a" B("dst"_"rgb", "src"_"rgb")) / "res"_"a"), ("res"_"a" = "dst"_"a" + "src"_"a"(1 - "dst"_"a")) :}
 * @enduml
 */
template <typename B>
Image<Color::RGBA8888> blendSeparable(Image<Color::RGBA8888> dst, const Image<Color::RGBA8888>& src, int x, int y,
                                      B&& blend) {
    return blendWith(std::move(dst), src, x, y,
                     [&](double sc, double sa, double dc, double da) {
                         return sc * sa * (1 - da) + dc * da * (1 - sa) + sa * da * blend(sc, dc);
                     },
                     unionOf);
}

/** what a blend written with blendPixels leaves where src does not reach */
enum class Outside {
    Kept,      /**< dst as it was */
    Cleared,   /**< nothing, as imagemagick 6 leaves it for the operators that keep only where src is */
};

/**
 * every pixel src covers, the clear ones too, since some operators change dst where
 * src has no alpha: blend(s, d) takes both as straight rgba from 0 to 1 and answers
 * the same, and what src does not reach is kept or cleared.
 */
template <typename B>
Image<Color::RGBA8888> blendPixels(Image<Color::RGBA8888> dst, const Image<Color::RGBA8888>& src, int x, int y,
                                   Outside outside, B&& blend) {
    for (int by = 0; by < dst.height; by++) {
        for (int bx = 0; bx < dst.width; bx++) {
            auto* under = dst.data.data() + (static_cast<size_t>(by) * static_cast<size_t>(dst.width) + static_cast<size_t>(bx)) * 4;
            if (bx < x || by < y || bx >= x + src.width || by >= y + src.height) {
                if (outside == Outside::Cleared) std::fill_n(under, 4, 0);
                continue;
            }
            const auto* over = src.data.data()
                             + (static_cast<size_t>(by - y) * static_cast<size_t>(src.width) + static_cast<size_t>(bx - x)) * 4;
            const std::array<double, 4> s{over[0] / 255.0, over[1] / 255.0, over[2] / 255.0, over[3] / 255.0};
            const std::array<double, 4> d{under[0] / 255.0, under[1] / 255.0, under[2] / 255.0, under[3] / 255.0};
            const std::array<double, 4> res = blend(s, d);
            under[3] = static_cast<uint8_t>(255 - quantumByteOf(1 - res[3]));
            for (int c = 0; c < 3; c++) under[c] = under[3] == 0 ? 0 : quantumByteOf(res[c]);
        }
    }
    return dst;
}

/** how bright a straight rgba pixel is, rec.709 as imagemagick 6 weighs it */
constexpr double intensityOf(const std::array<double, 4>& pixel) {
    return 0.212656 * pixel[0] + 0.715158 * pixel[1] + 0.072186 * pixel[2];
}

/** 1 / v, or as near as imagemagick lets it get when v is next to nothing */
constexpr double reciprocalOf(double v) {
    constexpr double epsilon = 1.0e-12;
    if (v >= epsilon || v <= -epsilon) return 1 / v;
    return v < 0 ? -1 / epsilon : 1 / epsilon;
}

}

namespace Haio::Implementation::Transforms {

/**
 * a layer laid on a picture: Transform<Operation::Composite, Color::RGBA8888>{}(base, layer, compose, gravity, x,
 * y), one class per colour the base can be, in
 * `library/implementation/transforms/composite/COLOUR.cpp`. the layer is always
 * rgba8888: it has come out of its own pipeline, and it is the layer's alpha that says
 * where it covers.
 */
template <Color P>
concept Composable = Util::MemberOf<Transform<Operation::Composite, P>, Transform<>>
    && requires (const Transform<Operation::Composite, P>& composite, Image<P> b, const Image<Color::RGBA8888>& l, Compose c, Transform<>::Gravity g, int x) {
        { composite(std::move(b), l, c, g, x, x) } -> std::same_as<Result<Image<P>>>;
    };

/** what every blend answers with */
using BlendRes = Result<Image<Color::RGBA8888>>;

/** the picture under, which the blend changes and hands back; rgba8888 unless a blend says */
template <Color P = Color::RGBA8888>
using BlendDst = Image<P>;

/** the picture laid on it */
using BlendSrc = const Image<Color::RGBA8888>&;

/** @cond */
template <Compose C = Compose::Over, Color Dst = Color::NIL, Color Src = Color::NIL> class Blend {};   // Dst and Src were Color::RGBA8888
/** @endcond */

/**
 * how a layer and the picture under it are mixed:
 * Blend<Compose::Multiply, Color::RGBA8888, Color::RGBA8888>{}(dst, src, x, y), one class
 * per operator and the colours it takes, in
 * `library/implementation/transforms/composite/blend/NAME.cpp`.
 */
template <> class Blend<> {
public:
    /** @cond */
    constexpr Blend() = default;
    virtual ~Blend() = default;
    /** @endcond */

    /** how a layer and the picture under it are mixed, Blend<>::Compose::Multiply */
    using Compose = Transforms::Compose;

    /** @cond */
    // the same with the operator named at runtime, both pictures in rgba8888
    BlendRes operator()(Compose compose, BlendDst<> dst, BlendSrc src, int x, int y) const;
    /** @endcond */
};

template <Compose C, Color Dst, Color Src>   // Dst and Src were Color::RGBA8888
concept Blendable = Util::MemberOf<Blend<C, Dst, Src>, Blend<>>
    && requires (const Blend<C, Dst, Src>& blend, BlendDst<Dst> d, const Image<Src>& s, int x) {
        { blend(std::move(d), s, x, x) } -> std::same_as<BlendRes>;
    };

}

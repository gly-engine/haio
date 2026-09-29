#pragma once

#include "haio_transform.hpp"
#include "haio/stage.hpp"

#include <algorithm>
#include <array>
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
 * how a layer and the picture under it are mixed, named as imagemagick's -compose
 * names them.
 *
 * @todo the operators that take `-define compose:args`, which the composite stage
 * cannot pass yet: dissolve and blend (a share of src and of dst), modulate
 * (brightness and saturation), and blur, displace and distort (src as a map that
 * resamples dst).
 */
enum class Compose {
    /**
     * @ref Haio::Transforms::Blend<Compose::Over> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose over -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose over -composite -resize 64x64 png:-
     */
    Over,
    /**
     * @ref Haio::Transforms::Blend<Compose::Multiply> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose multiply -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose multiply -composite -resize 64x64 png:-
     */
    Multiply,
    /**
     * @ref Haio::Transforms::Blend<Compose::Screen> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose screen -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose screen -composite -resize 64x64 png:-
     */
    Screen,
    /**
     * @ref Haio::Transforms::Blend<Compose::Overlay> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose overlay -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose overlay -composite -resize 64x64 png:-
     */
    Overlay,
    /**
     * @ref Haio::Transforms::Blend<Compose::Darken> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose darken -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose darken -composite -resize 64x64 png:-
     */
    Darken,
    /**
     * @ref Haio::Transforms::Blend<Compose::Lighten> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose lighten -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose lighten -composite -resize 64x64 png:-
     */
    Lighten,
    /**
     * @ref Haio::Transforms::Blend<Compose::ColorDodge> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose colordodge -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose colordodge -composite -resize 64x64 png:-
     */
    ColorDodge,
    /**
     * @ref Haio::Transforms::Blend<Compose::ColorBurn> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose colorburn -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose colorburn -composite -resize 64x64 png:-
     */
    ColorBurn,
    /**
     * @ref Haio::Transforms::Blend<Compose::HardLight> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose hardlight -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose hardlight -composite -resize 64x64 png:-
     */
    HardLight,
    /**
     * @ref Haio::Transforms::Blend<Compose::SoftLight> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose softlight -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose softlight -composite -resize 64x64 png:-
     */
    SoftLight,
    /**
     * @ref Haio::Transforms::Blend<Compose::Difference> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose difference -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose difference -composite -resize 64x64 png:-
     */
    Difference,
    /**
     * @ref Haio::Transforms::Blend<Compose::Exclusion> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose exclusion -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose exclusion -composite -resize 64x64 png:-
     */
    Exclusion,
    /**
     * @ref Haio::Transforms::Blend<Compose::Plus> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose plus -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose plus -composite -resize 64x64 png:-
     */
    Plus,
    /**
     * @ref Haio::Transforms::Blend<Compose::DstIn> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose dstin -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose dstin -composite -resize 64x64 png:-
     */
    DstIn,
    /**
     * @ref Haio::Transforms::Blend<Compose::SrcIn> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose srcin -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose srcin -composite -resize 64x64 png:-
     */
    SrcIn,
    /**
     * @ref Haio::Transforms::Blend<Compose::SrcOut> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose srcout -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose srcout -composite -resize 64x64 png:-
     */
    SrcOut,
    /**
     * @ref Haio::Transforms::Blend<Compose::SrcAtop> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose srcatop -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose srcatop -composite -resize 64x64 png:-
     */
    SrcAtop,
    /**
     * @ref Haio::Transforms::Blend<Compose::DstOver> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose dstover -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose dstover -composite -resize 64x64 png:-
     */
    DstOver,
    /**
     * @ref Haio::Transforms::Blend<Compose::DstOut> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose dstout -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose dstout -composite -resize 64x64 png:-
     */
    DstOut,
    /**
     * @ref Haio::Transforms::Blend<Compose::DstAtop> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose dstatop -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose dstatop -composite -resize 64x64 png:-
     */
    DstAtop,
    /**
     * @ref Haio::Transforms::Blend<Compose::Xor> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose xor -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose xor -composite -resize 64x64 png:-
     */
    Xor,
    /**
     * @ref Haio::Transforms::Blend<Compose::Clear> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose clear -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose clear -composite -resize 64x64 png:-
     */
    Clear,
    /**
     * @ref Haio::Transforms::Blend<Compose::MinusDst> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose minusdst -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose minusdst -composite -resize 64x64 png:-
     */
    MinusDst,
    /**
     * @ref Haio::Transforms::Blend<Compose::MinusSrc> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose minussrc -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose minussrc -composite -resize 64x64 png:-
     */
    MinusSrc,
    /**
     * @ref Haio::Transforms::Blend<Compose::DivideDst> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose dividedst -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose dividedst -composite -resize 64x64 png:-
     */
    DivideDst,
    /**
     * @ref Haio::Transforms::Blend<Compose::DivideSrc> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose dividesrc -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose dividesrc -composite -resize 64x64 png:-
     */
    DivideSrc,
    /**
     * @ref Haio::Transforms::Blend<Compose::ModulusAdd> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose modulusadd -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose modulusadd -composite -resize 64x64 png:-
     */
    ModulusAdd,
    /**
     * @ref Haio::Transforms::Blend<Compose::ModulusSubtract> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose modulussubtract -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose modulussubtract -composite -resize 64x64 png:-
     */
    ModulusSubtract,
    /**
     * @ref Haio::Transforms::Blend<Compose::LinearDodge> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose lineardodge -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose lineardodge -composite -resize 64x64 png:-
     */
    LinearDodge,
    /**
     * @ref Haio::Transforms::Blend<Compose::LinearBurn> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose linearburn -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose linearburn -composite -resize 64x64 png:-
     */
    LinearBurn,
    /**
     * @ref Haio::Transforms::Blend<Compose::LinearLight> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose linearlight -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose linearlight -composite -resize 64x64 png:-
     */
    LinearLight,
    /**
     * @ref Haio::Transforms::Blend<Compose::VividLight> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose vividlight -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose vividlight -composite -resize 64x64 png:-
     */
    VividLight,
    /**
     * @ref Haio::Transforms::Blend<Compose::PinLight> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose pinlight -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose pinlight -composite -resize 64x64 png:-
     */
    PinLight,
    /**
     * @ref Haio::Transforms::Blend<Compose::PegtopLight> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose pegtoplight -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose pegtoplight -composite -resize 64x64 png:-
     */
    PegtopLight,
    /**
     * @ref Haio::Transforms::Blend<Compose::HardMix> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose hardmix -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose hardmix -composite -resize 64x64 png:-
     */
    HardMix,
    /**
     * @ref Haio::Transforms::Blend<Compose::Hue> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose hue -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose hue -composite -resize 64x64 png:-
     */
    Hue,
    /**
     * @ref Haio::Transforms::Blend<Compose::Saturate> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose saturate -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose saturate -composite -resize 64x64 png:-
     */
    Saturate,
    /**
     * @ref Haio::Transforms::Blend<Compose::Luminize> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose luminize -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose luminize -composite -resize 64x64 png:-
     */
    Luminize,
    /**
     * @ref Haio::Transforms::Blend<Compose::Colorize> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose colorize -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose colorize -composite -resize 64x64 png:-
     */
    Colorize,
    /**
     * @ref Haio::Transforms::Blend<Compose::Src> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose src -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose src -composite -resize 64x64 png:-
     */
    Src,
    /**
     * @ref Haio::Transforms::Blend<Compose::Dst> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose dst -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose dst -composite -resize 64x64 png:-
     */
    Dst,
    /**
     * @ref Haio::Transforms::Blend<Compose::Copy> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copy -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copy -composite -resize 64x64 png:-
     */
    Copy,
    /**
     * @ref Haio::Transforms::Blend<Compose::CopyRed> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copyred -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copyred -composite -resize 64x64 png:-
     */
    CopyRed,
    /**
     * @ref Haio::Transforms::Blend<Compose::CopyGreen> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copygreen -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copygreen -composite -resize 64x64 png:-
     */
    CopyGreen,
    /**
     * @ref Haio::Transforms::Blend<Compose::CopyBlue> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copyblue -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copyblue -composite -resize 64x64 png:-
     */
    CopyBlue,
    /**
     * @ref Haio::Transforms::Blend<Compose::CopyCyan> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copycyan -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copycyan -composite -resize 64x64 png:-
     */
    CopyCyan,
    /**
     * @ref Haio::Transforms::Blend<Compose::CopyMagenta> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copymagenta -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copymagenta -composite -resize 64x64 png:-
     */
    CopyMagenta,
    /**
     * @ref Haio::Transforms::Blend<Compose::CopyYellow> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copyyellow -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copyyellow -composite -resize 64x64 png:-
     */
    CopyYellow,
    /**
     * @ref Haio::Transforms::Blend<Compose::CopyBlack> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copyblack -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copyblack -composite -resize 64x64 png:-
     */
    CopyBlack,
    /**
     * @ref Haio::Transforms::Blend<Compose::CopyOpacity> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copyopacity -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copyopacity -composite -resize 64x64 png:-
     */
    CopyOpacity,
    /**
     * @ref Haio::Transforms::Blend<Compose::DarkenIntensity> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose darkenintensity -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose darkenintensity -composite -resize 64x64 png:-
     */
    DarkenIntensity,
    /**
     * @ref Haio::Transforms::Blend<Compose::LightenIntensity> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose lightenintensity -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose lightenintensity -composite -resize 64x64 png:-
     */
    LightenIntensity,
    /**
     * @ref Haio::Transforms::Blend<Compose::ChangeMask> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose changemask -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose changemask -composite -resize 64x64 png:-
     */
    ChangeMask,
    /**
     * @ref Haio::Transforms::Blend<Compose::Stereo> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose stereo -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose stereo -composite -resize 64x64 png:-
     */
    Stereo,
    /**
     * @ref Haio::Transforms::Blend<Compose::Bumpmap> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose bumpmap -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose bumpmap -composite -resize 64x64 png:-
     */
    Bumpmap,
    /**
     * @ref Haio::Transforms::Blend<Compose::Tint> @n
     * @ref Haio::Transforms::Blend<Compose::Tint, Color::GRAYALPHA88> @n
     * @ref Haio::Transforms::Blend<Compose::Tint, Color::GRAY8> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose tint -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose tint -composite -resize 64x64 png:-
     */
    Tint,
};

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

/** what every blend answers with */
using BlendRes = Result<Image<Color::RGBA8888>>;

/** the picture under, which the blend changes and hands back; rgba8888 unless a blend says */
template <Color P = Color::RGBA8888>
using BlendDst = Image<P>;

/** the picture laid on it */
using BlendSrc = const Image<Color::RGBA8888>&;

/** @cond */
template <Compose C, Color Dst = Color::RGBA8888, Color Src = Color::RGBA8888>
BlendRes Blend(BlendDst<Dst> dst, const Image<Src>& src, int x, int y) = delete;
/** @endcond */

template <Compose C, Color Dst = Color::RGBA8888, Color Src = Color::RGBA8888>
concept Blendable = requires (BlendDst<Dst> d, const Image<Src>& s, int x) {
    { Blend<C, Dst, Src>(std::move(d), s, x, x) } -> std::same_as<BlendRes>;
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

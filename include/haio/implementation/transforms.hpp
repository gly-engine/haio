#pragma once

#include "haio/implementation/wip/dither.hpp"
#include "haio/implementation/error.hpp"
#include "haio/implementation/image.hpp"

#include <utility>
#include <vector>

/**
 * geometry on addressable pixels. the colour no longer has to be rgba8888: the only
 * thing these ever needed from it was how wide a pixel is, which PixelStride answers,
 * so a crop on rgb565 stops going through a conversion it never used.
 *
 * the transforms themselves are in include/haio/implementation/transforms/, one
 * header each, and the colours each one runs on are the sources in
 * `library/implementation/transforms/NAME/`.
 */
namespace Haio::Implementation {

/**
 * a transformed picture keeps everything about the original except its pixels.
 *
 * building a fresh Image would drop whatever else the colour carries, which for a
 * palette is the palette: a cropped picture would come back as indices into nothing.
 */
template <Color P>
Image<P> withPixels(const Image<P>& from, int width, int height, std::vector<uint8_t> pixels) {
    Image<P> out = from;
    out.width = width;
    out.height = height;
    out.data = std::move(pixels);
    return out;
}

}

/** the transforms' family sees the implementation's names as they are there */
namespace Haio::Implementation::Transforms {

/**
 * how a layer and the picture under it are mixed, named as imagemagick's -compose
 * names them.
 *
 * @todo the operators that take `-define compose:args`, which the composite stage
 * cannot pass yet: dissolve and blend (a share of src and of dst), modulate
 * (brightness and saturation), and blur, displace and distort (src as a map that
 * resamples dst).
 *
 * @note photoshop's dissolve is not imagemagick's: it shows each pixel or not at
 * random, as likely as its opacity, where imagemagick fades by a share.
 */
enum class Compose {
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::Over, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose over -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose over -composite -resize 64x64 png:-
     */
    Over,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::Multiply, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose multiply -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose multiply -composite -resize 64x64 png:-
     */
    Multiply,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::Screen, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose screen -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose screen -composite -resize 64x64 png:-
     */
    Screen,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::Overlay, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose overlay -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose overlay -composite -resize 64x64 png:-
     */
    Overlay,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::Darken, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose darken -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose darken -composite -resize 64x64 png:-
     */
    Darken,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::Lighten, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose lighten -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose lighten -composite -resize 64x64 png:-
     */
    Lighten,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::ColorDodge, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose colordodge -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose colordodge -composite -resize 64x64 png:-
     */
    ColorDodge,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::ColorBurn, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose colorburn -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose colorburn -composite -resize 64x64 png:-
     */
    ColorBurn,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::HardLight, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose hardlight -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose hardlight -composite -resize 64x64 png:-
     */
    HardLight,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::SoftLight, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose softlight -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose softlight -composite -resize 64x64 png:-
     */
    SoftLight,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::Difference, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose difference -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose difference -composite -resize 64x64 png:-
     */
    Difference,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::Exclusion, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose exclusion -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose exclusion -composite -resize 64x64 png:-
     */
    Exclusion,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::Plus, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose plus -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose plus -composite -resize 64x64 png:-
     */
    Plus,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::DstIn, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose dstin -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose dstin -composite -resize 64x64 png:-
     */
    DstIn,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::SrcIn, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose srcin -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose srcin -composite -resize 64x64 png:-
     */
    SrcIn,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::SrcOut, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose srcout -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose srcout -composite -resize 64x64 png:-
     */
    SrcOut,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::SrcAtop, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose srcatop -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose srcatop -composite -resize 64x64 png:-
     */
    SrcAtop,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::DstOver, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose dstover -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose dstover -composite -resize 64x64 png:-
     */
    DstOver,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::DstOut, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose dstout -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose dstout -composite -resize 64x64 png:-
     */
    DstOut,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::DstAtop, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose dstatop -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose dstatop -composite -resize 64x64 png:-
     */
    DstAtop,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::Xor, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose xor -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose xor -composite -resize 64x64 png:-
     */
    Xor,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::Clear, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose clear -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose clear -composite -resize 64x64 png:-
     */
    Clear,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::MinusDst, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose minusdst -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose minusdst -composite -resize 64x64 png:-
     */
    MinusDst,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::MinusSrc, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose minussrc -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose minussrc -composite -resize 64x64 png:-
     */
    MinusSrc,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::DivideDst, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose dividedst -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose dividedst -composite -resize 64x64 png:-
     */
    DivideDst,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::DivideSrc, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose dividesrc -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose dividesrc -composite -resize 64x64 png:-
     */
    DivideSrc,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::ModulusAdd, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose modulusadd -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose modulusadd -composite -resize 64x64 png:-
     */
    ModulusAdd,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::ModulusSubtract, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose modulussubtract -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose modulussubtract -composite -resize 64x64 png:-
     */
    ModulusSubtract,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::LinearDodge, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose lineardodge -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose lineardodge -composite -resize 64x64 png:-
     */
    LinearDodge,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::LinearBurn, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose linearburn -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose linearburn -composite -resize 64x64 png:-
     */
    LinearBurn,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::LinearLight, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose linearlight -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose linearlight -composite -resize 64x64 png:-
     */
    LinearLight,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::VividLight, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose vividlight -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose vividlight -composite -resize 64x64 png:-
     */
    VividLight,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::PinLight, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose pinlight -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose pinlight -composite -resize 64x64 png:-
     */
    PinLight,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::PegtopLight, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose pegtoplight -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose pegtoplight -composite -resize 64x64 png:-
     */
    PegtopLight,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::HardMix, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose hardmix -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose hardmix -composite -resize 64x64 png:-
     */
    HardMix,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::Hue, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose hue -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose hue -composite -resize 64x64 png:-
     */
    Hue,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::Saturate, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose saturate -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose saturate -composite -resize 64x64 png:-
     */
    Saturate,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::Luminize, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose luminize -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose luminize -composite -resize 64x64 png:-
     */
    Luminize,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::Colorize, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose colorize -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose colorize -composite -resize 64x64 png:-
     */
    Colorize,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::Src, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose src -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose src -composite -resize 64x64 png:-
     */
    Src,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::Dst, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose dst -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose dst -composite -resize 64x64 png:-
     */
    Dst,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::Copy, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copy -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copy -composite -resize 64x64 png:-
     */
    Copy,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::CopyRed, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copyred -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copyred -composite -resize 64x64 png:-
     */
    CopyRed,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::CopyGreen, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copygreen -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copygreen -composite -resize 64x64 png:-
     */
    CopyGreen,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::CopyBlue, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copyblue -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copyblue -composite -resize 64x64 png:-
     */
    CopyBlue,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::CopyCyan, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copycyan -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copycyan -composite -resize 64x64 png:-
     */
    CopyCyan,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::CopyMagenta, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copymagenta -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copymagenta -composite -resize 64x64 png:-
     */
    CopyMagenta,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::CopyYellow, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copyyellow -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copyyellow -composite -resize 64x64 png:-
     */
    CopyYellow,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::CopyBlack, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copyblack -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copyblack -composite -resize 64x64 png:-
     */
    CopyBlack,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::CopyOpacity, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose copyopacity -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose copyopacity -composite -resize 64x64 png:-
     */
    CopyOpacity,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::DarkenIntensity, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose darkenintensity -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose darkenintensity -composite -resize 64x64 png:-
     */
    DarkenIntensity,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::LightenIntensity, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose lightenintensity -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose lightenintensity -composite -resize 64x64 png:-
     */
    LightenIntensity,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::ChangeMask, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose changemask -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose changemask -composite -resize 64x64 png:-
     */
    ChangeMask,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::Stereo, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose stereo -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose stereo -composite -resize 64x64 png:-
     */
    Stereo,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::Bumpmap, Color::RGBA8888, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose bumpmap -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose bumpmap -composite -resize 64x64 png:-
     */
    Bumpmap,
    /**
     * @ref Haio::Implementation::Transforms::Blend<Compose::Tint, Color::RGBA8888, Color::RGBA8888> @n
     * @ref Haio::Implementation::Transforms::Blend<Compose::Tint, Color::GRAYALPHA88, Color::RGBA8888> @n
     * @ref Haio::Implementation::Transforms::Blend<Compose::Tint, Color::GRAY8, Color::RGBA8888> @n
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256color.png -compose tint -composite -resize 64x64 png:-
     * @haio{convert} assets/jucagato256x256.png assets/disk256x256gray.png -compose tint -composite -resize 64x64 png:-
     */
    Tint,
};

/**
 * what a Transform does to a picture, which is what picks the class:
 * Transform<Operation::Resize, Color::RGBA8888>.
 */
enum class Operation {
    Crop,
    Resize,
    Negate,
    Radius,
    Composite,
};

/** @cond */
template <Operation O = Operation::Crop, Color P = Color::NIL> class Transform {};
/** @endcond */

/**
 * a picture changed: Transform<Operation::Crop, Color::RGBA8888>{}(image, rect), one class
 * per operation and colour it runs on, in
 * `library/implementation/transforms/OPERATION/COLOUR.cpp`. what each operation
 * is handed besides the picture is its own, and the names those take are here.
 */
template <> class Transform<> {
public:
    /** @cond  */
    constexpr Transform() = default;
    virtual ~Transform() = default;
    /** @endcond */

    /** what a pair does, Transform<>::Operation::Resize */
    using Operation = Transforms::Operation;

    /** how a layer and the picture under it are mixed, Transform<>::Compose::Over */
    using Compose = Transforms::Compose;

    /**
     * how a resize picks its pixels, named the way imagemagick names its -filter.
     *
     * the names people type are the enumerators, lowered, so a new filter is an
     * enumerator here, a kernel beside resize/nearest.hpp and a case in each colour's
     * source -- and nothing in the parser or the grammar.
     */
    enum class ResizeFilter {
        Point,   /**< nearest neighbour: no mixing, so it runs on anything addressable */
    };

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
};

}

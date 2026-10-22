#pragma once

#include "haio_codec.hpp"
#include "haio_string.hpp"
#include "haio_util.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

/**
 * what every brush has in common: the size it paints at, the colours it paints
 * with, and how two of them are mixed. imagemagick's generators agree on all three,
 * so haio's brushes do too.
 */
namespace Haio::Codecs::Canvas {

/** -size is optional, the way imagemagick has it: without one a canvas is 1x1 */
inline constexpr Stages::Option size{
    .spellings = {"size"}, .takes = "size", .help = "the size it is drawn at",
    .shape = Stages::Shape::Size, .fallback = "1x1",
};

/**
 * the same -size for a code, which has a size of its own when nobody says one: a
 * pixel a module, the code and no margin. told apart only so the usage says so.
 */
inline constexpr Stages::Option moduleSize{
    .spellings = {"size"}, .takes = "size",
    .help = "the size it is drawn at, a whole number of pixels a module; one a module when nobody says",
    .shape = Stages::Shape::Size,
};

/** a side past this is a typo, not a picture, and would ask for more memory than there is */
inline constexpr int largest = 16384;

inline Result<Size> sizeOf(const Settings& settings) {
    const auto* wanted = settingNamed(settings, size.name());
    const std::string_view spelled = wanted ? std::string_view{wanted->value} : size.fallback;
    if (const auto why = Stages::refusal(size, spelled)) HAIO_FAIL(InvalidInput, *why);

    Size out;
    if (!String::tryGetSize(spelled, out)) HAIO_FAIL(InvalidInput, "invalid argument for option `-size': " + std::string(spelled));
    if (out.width > largest || out.height > largest) {
        HAIO_FAIL(InvalidInput, "width or height exceeds limit of " + std::to_string(largest) + " " + Stages::quoted(spelled));
    }
    return out;
}

inline Result<uint32_t> colourOf(std::string_view text) {
    const auto colour = Util::GetColorFromName(text);
    if (!colour) HAIO_FAIL(InvalidInput, "unrecognized color " + Stages::quoted(text));
    return *colour;
}

/** the ink and the paper, named the way imagemagick's -fill and -background are */
inline constexpr Stages::Option fill{
    .spellings = {"fill"}, .takes = "colour", .help = "the colour it is painted in", .fallback = "black",
};

inline constexpr Stages::Option background{
    .spellings = {"background"}, .takes = "colour", .help = "the colour behind it", .fallback = "white",
};

/** a colour option, or its fallback when nobody said */
inline Result<uint32_t> colourSetting(const Settings& settings, const Stages::Option& option) {
    const auto* setting = settingNamed(settings, option.name());
    return colourOf(setting ? std::string_view{setting->value} : option.fallback);
}

/** how bright a colour is, the way imagemagick weighs it: rec.709 on the stored values */
constexpr double intensityOf(uint32_t argb) {
    return (0.212656 * ((argb >> 16) & 0xFF) + 0.715158 * ((argb >> 8) & 0xFF) + 0.072186 * (argb & 0xFF)) / 255.0;
}

/**
 * "red-blue", "red", or nothing at all: the two ends of a blend.
 *
 * nothing is the default pair. one colour gets the other end imagemagick gives it --
 * black when it is bright and white when it is not -- so "gradient:blue" fades to
 * white and "gradient:#ff8800" to black.
 */
inline Result<std::pair<uint32_t, uint32_t>> pairOf(std::string_view text, uint32_t first, uint32_t second) {
    if (text.empty()) return std::pair{first, second};

    const auto dash = text.find('-');
    HAIO_TRY(from, colourOf(text.substr(0, dash)));
    if (dash == std::string_view::npos) {
        return std::pair{from, intensityOf(from) > 0.5 ? 0xFF000000u : 0xFFFFFFFFu};
    }
    HAIO_TRY(to, colourOf(text.substr(dash + 1)));
    return std::pair{from, to};
}

/**
 * a blended value back into a byte the way imagemagick 6 at sixteen bits does it:
 * rounded to its quantum, and that quantum then cut down to eight bits. going
 * through the quantum is what makes 218.99999 a 219 and 127.5 a 127, and matching it
 * is what makes a haio gradient the same picture, pixel for pixel.
 */
constexpr uint32_t byteOf(double value) {
    const auto quantum = static_cast<uint32_t>(std::clamp(value * 257.0 + 0.5, 0.0, 65535.0));
    return quantum / 257;
}

/**
 * a share of the way from one colour to the other, blended as imagemagick 6 does:
 * the colours weighted by how opaque each end is, and the transparency -- which is
 * what it keeps, rather than alpha -- blended on its own.
 */
constexpr uint32_t mix(uint32_t from, uint32_t to, double share) {
    const auto channel = [](uint32_t argb, int shift) { return static_cast<double>((argb >> shift) & 0xFF); };

    const double fromAlpha = channel(from, 24) / 255 * (1 - share);
    const double toAlpha = channel(to, 24) / 255 * share;
    const double coverage = fromAlpha + toAlpha;

    const double clear = (255 - channel(from, 24)) * (1 - share) + (255 - channel(to, 24)) * share;
    uint32_t out = (255 - byteOf(clear)) << 24;
    if (coverage <= 0) return out;

    for (int shift = 0; shift < 24; shift += 8) {
        out |= byteOf((channel(from, shift) * fromAlpha + channel(to, shift) * toAlpha) / coverage) << shift;
    }
    return out;
}

inline Image<Color::RGBA8888> blank(Size size) {
    return Image<Color::RGBA8888>{size.width, size.height,
                                  std::vector<uint8_t>(static_cast<size_t>(size.width) * static_cast<size_t>(size.height) * 4)};
}

inline void put(Image<Color::RGBA8888>& image, int x, int y, uint32_t argb) {
    auto* pixel = image.data.data() + (static_cast<size_t>(y) * static_cast<size_t>(image.width) + static_cast<size_t>(x)) * 4;
    pixel[0] = static_cast<uint8_t>(argb >> 16);
    pixel[1] = static_cast<uint8_t>(argb >> 8);
    pixel[2] = static_cast<uint8_t>(argb);
    pixel[3] = static_cast<uint8_t>(argb >> 24);
}

/** a number as a -define writes one: gradient:angle=22.5 */
inline std::optional<double> numberOf(std::string_view text) {
    double value = 0;
    const auto [end, problem] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (problem != std::errc{} || end != text.data() + text.size()) return std::nullopt;
    return value;
}

/** "x,y", as gradient:center and gradient:radii write a pair */
inline std::optional<std::pair<double, double>> pointOf(std::string_view text) {
    const auto comma = text.find(',');
    if (comma == std::string_view::npos) return std::nullopt;
    const auto x = numberOf(text.substr(0, comma));
    const auto y = numberOf(text.substr(comma + 1));
    if (!x || !y) return std::nullopt;
    return std::pair{*x, *y};
}

inline Error invalidDefine(const Setting& setting) {
    return Error{ErrorCode::InvalidInput, "invalid argument for option `-define': " + setting.name + "=" + setting.value};
}


}

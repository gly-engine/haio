#pragma once

#include "haio_common.hpp"
#include "haio_formats.hpp"
#include "haio_object.hpp"

#include <expected>
#include <meta>
#include <optional>
#include <span>

namespace Haio {

using Bytes = std::span<const uint8_t>;

enum class ErrorCode {
    Internal,
    InvalidInput,
    UnsupportedFormat,
    NotFound,
    Io,
    Upstream,
    Timeout,
};

struct Error {
    ErrorCode code = ErrorCode::Internal;
    std::string message;
};

template <typename T>
using Result = std::expected<T, Error>;

/** pixels in memory. it has a colour and no container, because it is not a file yet */
template <Color P>
struct Image {
    static constexpr Color color = P;
    int width = 0;
    int height = 0;
    std::vector<uint8_t> data;
};

/**
 * how wide one pixel is, or zero when the colour has no such number: etc1 packs four
 * by four blocks and yuv420 splits into planes, so neither can be measured a pixel at
 * a time. sizeOf reads this too, so the widths are written down once.
 */
constexpr size_t strideOf(Color color) {
    switch (color) {
        case Color::RGBA8888: return 4;
        case Color::RGB888:   return 3;
        case Color::RGB565:   return 2;
        case Color::GRAY8:    return 1;
        // one byte per pixel, so a crop or a resize can index it like any other
        case Color::PALETTE:  return 1;
        // tiled and planar, so no pixel has an address of its own
        case Color::CHR_NES:  break;
        case Color::ETC1:     break;
        case Color::YUV420:   break;
    }
    return 0;
}

/** a transform can index these; having no stride is exactly what rules the others out */
template <Color P> concept Addressable = strideOf(P) != 0;

/**
 * which byte of a pixel holds its alpha, or a negative when the colour has none.
 * rgba8888 is the only one today; the rule is written as a rule so that the day a
 * second one arrives, rgba4444 in a ktx or a palette that keeps a transparent entry,
 * it is one line here rather than a second copy of the masking loop.
 */
constexpr int alphaOffsetOf(Color color) {
    switch (color) {
        case Color::RGBA8888: return 3;
        case Color::RGB888:   break;
        case Color::RGB565:   break;
        case Color::GRAY8:    break;
        // tiled and planar, so no pixel has an address of its own
        case Color::CHR_NES:  break;
        case Color::ETC1:     break;
        case Color::YUV420:   break;
    }
    return -1;
}

/** and these can have a corner rounded away, because there is an alpha to clear */
template <Color P> concept Maskable = Addressable<P> && alphaOffsetOf(P) >= 0;

/**
 * the one colour that is not self describing: a byte per pixel means nothing without
 * the colours it points at, so the palette travels with the picture.
 *
 * it is a specialisation rather than a member on every Image, because every other
 * colour would carry an empty vector around for nothing. the cost is that Move, which
 * moves spans of bytes, has no way to carry the entries: palette conversions go
 * through Convert directly and never through convertVia.
 */
template <>
struct Image<Color::PALETTE> {
    static constexpr Color color = Color::PALETTE;
    int width = 0;
    int height = 0;
    std::vector<uint8_t> data;

    /** 0xAARRGGBB, at most 256 of them, and an index past the end is an error */
    std::vector<uint32_t> entries;
};

/** bytes of a file. it has a container and the colour it was found to hold */
struct Blob {
    Format format = Format::RAW;
    Color color = Color::RGBA8888;
    std::string contentType = "application/octet-stream";
    std::string path;
    std::vector<uint8_t> data;
};

/**
 * the verbs. they live apart from the vocabulary above because the runtime facade in
 * haio_registry.hpp offers Decode(const Blob&) and Detect(Bytes) with the very same
 * parameter lists: without a namespace between them, Decode(blob) and
 * Decode<Format::PNG>(blob) read alike and do different things.
 */
namespace Codecs {

/**
 * the colour a format carries when nobody says otherwise. only formats with a real
 * answer declare it: a ktx2 holds any of three, so Decode<Format::KTX2> alone is a
 * compile error telling the caller to be specific.
 */
template <Format F> struct DefaultColor;

/**
 * deleted on purpose. a codec declares only the pairs it can do, and naming one it
 * left out is ill-formed, which is what lets the concepts below answer for themselves
 * instead of failing at link time. drop a codec from the build and its format stays
 * in the enum, naming itself, with every capability gone.
 */
template <Format F, Color P = DefaultColor<F>::value> bool             Detect(Bytes)       = delete;
template <Format F, Color P = DefaultColor<F>::value> Result<Image<P>> Decode(const Blob&) = delete;
template <Format F, Color P>                          Result<Blob>     Encode(Image<P>)    = delete;

/**
 * @name Convert
 * @{
 */
/** colour to colour, no file involved. the heavy ones live behind Move */
template <Color From, Color To> Result<Image<To>> Convert(Image<From>) = delete;
/**
 * @}
 */

/**
 * the hot loop under Convert, and what a codec calls when it wants to write pixels
 * straight into a buffer it already owns. dst is sized by the caller so the simd
 * paths can rely on it, and size is here because block formats like etc1 cannot be
 * transformed pixel by pixel.
 *
 * it reports rather than assumes: the loop runs once per image, so one Result on the
 * way out costs nothing next to the work inside it.
 */
template <Color From, Color To> Result<void> Move(Bytes src, std::span<uint8_t> dst, Size size) = delete;

/**
 * @defgroup detect Detect
 * every container and colour pair this build can recognise. these are exactly the
 * specialisations that satisfy Detectable, so a codec dropped from the build leaves
 * this page and the concept together, while its format keeps its name in the enum.
 */

/**
 * @defgroup decode Decode
 * every pair this build can read into pixels. a format may be recognised here and
 * still be missing from @ref decode, which is how haio names a file it cannot open.
 */

/**
 * @defgroup encode Encode
 * every pair this build can write out. the colour is the one stored inside the
 * container, not the one the caller happens to hold.
 */

/**
 * @defgroup convert Convert
 * colour to colour with a whole image in hand. each one delegates to convertVia,
 * which sizes the destination and hands the work to the matching @ref move.
 */

/**
 * @defgroup move Move
 * the hot loops, writing pixels into a buffer the caller already owns. this is where
 * the simd lives.
 */

/**
 * @ingroup detect
 * can these bytes be recognised as this pair, asked of the pair rather than answered by hand.
 */
template <Format F, Color P> concept Detectable = requires (Bytes d)      { { Detect<F, P>(d) } -> std::same_as<bool>; };
/**
 * @ingroup decode
 * can this pair be read into pixels, asked of the pair rather than answered by hand.
 */
template <Format F, Color P> concept Decodable  = requires (const Blob& b) { { Decode<F, P>(b) } -> std::same_as<Result<Image<P>>>; };
/**
 * @ingroup encode
 * can an image of this colour be written into this container, asked of the pair rather than answered by hand.
 */
template <Format F, Color P> concept Encodable  = requires (Image<P> i)    { { Encode<F, P>(std::move(i)) } -> std::same_as<Result<Blob>>; };
/**
 * @ingroup move
 * is there a loop from one colour to the other, asked of the pair rather than answered by hand.
 */
template <Color From, Color To> concept Movable = requires (Bytes s, std::span<uint8_t> d, Size z) { { Move<From, To>(s, d, z) } -> std::same_as<Result<void>>; };
/**
 * @ingroup convert
 * is there a whole image conversion from one colour to the other, asked of the pair rather than answered by hand.
 */
template <Color From, Color To> concept Convertible = requires (Image<From> i) { { Convert<From, To>(std::move(i)) } -> std::same_as<Result<Image<To>>>; };

}

/**
 * go style early return. c++26 still has no "?" operator, so unwrapping a Result
 * and leaving on failure is either this or a monadic chain that reads badly when
 * the steps are independent.
 */
#define HAIO_TRY(name, expr)                                          \
    auto name##Result = (expr);                                       \
    if (!name##Result) return std::unexpected(name##Result.error());  \
    auto& name = *name##Result

/** for a check that only reports a problem and produces no value */
#define HAIO_CHECK(expr)                                              \
    do {                                                              \
        if (auto problem = (expr)) return std::unexpected(*problem);  \
    } while (false)

/** a plain failure, for the common case of a message and a cause */
#define HAIO_FAIL(why, message) \
    return std::unexpected(::Haio::Error{::Haio::ErrorCode::why, message})

/** the enums are the loops: a new enumerator grows every table on its own */
#define HAIO_FOR_EACH_FORMAT(e) \
    template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^Haio::Format)))

#define HAIO_FOR_EACH_COLOR(e) \
    template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^Haio::Color)))

/** the enumerator is PNG, the name people type is png */
consteval std::string_view lowerOf(std::meta::info enumerator) {
    std::string out{std::meta::identifier_of(enumerator)};
    for (auto& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return std::define_static_string(out);
}

}

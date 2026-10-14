#pragma once

#include "haio/domain.hpp"
#include "haio/implementation/error.hpp"
#include "haio/implementation/geometry.hpp"
#include "haio/implementation/image.hpp"
#include "haio/implementation/option.hpp"
#include "haio/util/family.hpp"
#include "haio/util/option.hpp"
#include "haio/util/reflection.hpp"

#include <array>
#include <concepts>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace Haio::Implementation {

/** what a file turned out to be: the container and the colour it holds */
struct Found {
    Format format = Format::RAW;
    Color color = Color::RGBA8888;

    explicit operator bool() const noexcept { return format != Format::RAW; }
};

/**
 * an image whose colour is a value rather than a type, which is what the runtime path
 * holds between a decode and an encode. it is the same thing as an Image<P> with the
 * P moved to where a string from a command line can reach it.
 *
 * a palette never travels like this: its entries have nowhere to go, so a picture
 * that decodes to one is expanded on the way in.
 */
struct AnyImage {
    Color color = Color::RGBA8888;
    int width = 0;
    int height = 0;
    std::vector<uint8_t> data;
};

}

/** the parts of a codec that are still being worked out */
namespace Haio::Implementation::WIP {

/**
 * the colour a format carries when nobody says otherwise, which is the one Encode<>
 * writes when nobody names one. only formats with a real answer declare it: a ktx2
 * holds any of three, so it declares none.
 */
template <Format F> struct DefaultColor;

}

/**
 * the verbs, one family of classes each, and what a codec declares it reads. what
 * they are handed and answer with is the implementation's, so its names are seen here
 * as they are there.
 */
namespace Haio::Implementation::Codecs {


// what every codec is handed and answers with, named once so a signature reads as
// what it does: CodecRes<P> Decode(CodecIn), CodecOut Encode(Image<P>)

/** the file a decode reads */
using CodecIn = const Blob&;

/** what a decode, a generate or a convert answers with: the pixels, or why not */
template <Color P>
using CodecRes = Result<Image<P>>;

/** what an encode answers with: the file, or why not */
using CodecOut = Result<Blob>;

// every verb below is a family of classes. the primary is empty on purpose: a codec
// specialises only the pairs it can do, and a pair it left out is not one of the
// family, which is what lets the concepts answer for themselves. drop a codec from the
// build and its format stays in the enum, naming itself, with every capability gone.
//
// the verb with no arguments, Detect<>, is the family: every pair derives from it, and
// its own operator() is the one to call when the pair is only known at runtime, trying
// each child in turn. the pairs are written by scripts/gen_scaffold.cpp from the
// operator() each codec defines in its source, so a new pair is a new definition and
// nothing else. the primaries are left out of the docs, where X<> stands for them.

/** @cond */
template <Format F = Format::RAW, Color P = Color::NIL> class Detect {};
/** @endcond */

/**
 * whether some bytes are a pair: Detect<Format::PNG, Color::RGBA8888>{}(bytes) is true
 * or false, and Detect<>{}(bytes) tries each pair in the order the enums declare them
 * and answers with the first that says yes, or with nothing.
 */
template <> class Detect<> {
public:
    /** @cond */
    constexpr Detect() = default;
    virtual ~Detect() = default;
    /** @endcond */

    Found operator()(Bytes data) const;
};

/** @cond */
template <Format F = Format::RAW, Color P = Color::NIL> class Decode {};   // P was DefaultColor<F>::value
/** @endcond */

/**
 * a file read into pixels: Decode<Format::PNG, Color::RGB888>{}(blob) for one pair, with
 * the settings as a second argument for a codec that reads them, as the jpeg quality
 * or a canvas's size. a codec writes one form or the other for a pair, never both.
 *
 * Decode<>{}(blob, settings) is the runtime way in: it asks Detect<> when the blob does
 * not say what it is, and keeps the colour the file was stored in so that whatever
 * comes next can start from it. a palette is expanded to rgba8888 on the way.
 */
template <> class Decode<> {
public:
    /** @cond */
    constexpr Decode() = default;
    virtual ~Decode() = default;
    /** @endcond */

    Result<AnyImage> operator()(CodecIn blob, const Settings& settings = {}) const;
};

/** @cond */
template <Format F = Format::RAW, Color P = Color::NIL> class Encode {};
/** @endcond */

/**
 * pixels written into a file: Encode<Format::KTX2, Color::ETC1>{}(image), with the
 * settings as a second argument for a codec that reads them. the colour is the one
 * stored inside the container, not the one the caller happens to hold.
 */
template <> class Encode<> {
public:
    /** @cond */
    constexpr Encode() = default;
    virtual ~Encode() = default;
    /** @endcond */

    /** @cond */
    // the runtime way out. the colour stored inside is the one asked for, or the one
    // the container calls its own when nobody asked, and the image gets there from
    // whatever colour it is in by the shortest road Convert<> knows. naming it is what
    // -pix_fmt does, and it is the only way to say "a ktx2 holding etc1" from a command line
    Result<Blob> operator()(AnyImage image, Format to, std::optional<Color> as = std::nullopt,
                            const Settings& settings = {}) const;

    // the same way out, for a caller holding rgba8888
    Result<Blob> operator()(Image<Color::RGBA8888> image, Format to, std::optional<Color> as = std::nullopt) const;

    // the indexed way out, for the containers that store a palette rather than expanding
    // it. there is no conversion into it here on purpose: haio does not pick colours for
    // anybody, so the picture arrives already fitted to a palette or not at all
    Result<Blob> operator()(Image<Color::PALETTE> image, Format to, const Settings& settings = {}) const;
    /** @endcond */
};

/**
 * what paints a picture out of nothing, the way a Format reads one out of bytes:
 * "xc:white", "gradient:red-blue". imagemagick calls these generators and files them
 * with its coders; they are kept apart here because they have no bytes to recognise
 * and nothing to write, and making them a Format meant pretending otherwise.
 *
 * the name on the command line is the enumerator in lower case with a dash between
 * its words, so RadialGradient is radial-gradient. any other name a brush answers to
 * is declared with it, in include/haio/implementation/codecs/generators/.
 */
enum class Brush {
    Xc,               /**< one colour */
    Null,             /**< nothing, transparent */
    Gradient,         /**< a linear blend, top to bottom unless told otherwise */
    RadialGradient,   /**< the same blend from the middle out */
    Plasma,           /**< a gradient broken up by noise */
    Hald,             /**< an identity colour lookup table */
    Code,             /**< a barcode, code 128 unless -format says another */
    Qr,               /**< a qr code, or a data matrix when -format says so */
    Text,             /**< words in a font, black on white */
};

/** @cond */
template <Brush B = Brush::Xc, Color P = Color::NIL> class Generate {};
/** @endcond */

/**
 * a picture painted out of nothing by a Brush, in the colour it is painted in:
 * Generate<Brush::Xc, Color::RGBA8888>{}(words, settings). the words are what came after
 * its colon on the command line, "red-blue" for "gradient:red-blue", and the settings
 * its options, -size among them.
 */
template <> class Generate<> {
public:
    /** @cond */
    constexpr Generate() = default;
    virtual ~Generate() = default;
    /** @endcond */

    /** the brush a pair paints with, Generate<>::Brush::Qr */
    using Brush = Codecs::Brush;

    // the names the brushes' own options take, which only the brushes read

    /**
     * the barcodes code: paints, named on the command line the way people write them:
     * "Code 128", "CODE-128" and "code128" are all one, since case, spaces and dashes
     * do not matter to a name.
     */
    enum class Barcode {
        Code128,
        Code39,
        Codabar,
        Ean13,
        Ean8,
        UpcA,
    };

    /** which way a linear gradient runs, named the way imagemagick's gradient:direction is */
    enum class GradientDirection {
        North,
        NorthEast,
        East,
        SouthEast,
        South,
        SouthWest,
        West,
        NorthWest,
    };

    /** how far a radial gradient reaches, as imagemagick's gradient:extent names it */
    enum class GradientExtent {
        Circle,     /**< the larger half of the picture, which is also Maximum */
        Diagonal,   /**< out to the corners */
        Ellipse,    /**< half the width one way and half the height the other */
        Maximum,
        Minimum,    /**< the smaller half */
    };

    /** the two dimensional codes qr: paints; qr code unless -format says data matrix */
    enum class Matrix {
        QrCode,
        DataMatrix,
    };

    /** how much of a qr code can be lost and still read, from about 7% to about 30% */
    enum class QrLevel {
        L,
        M,
        Q,
        H,
    };

    /** @cond */
    // paints with a brush named at runtime, in the first colour it paints in
    Result<AnyImage> operator()(Brush brush, std::string_view words, const Settings& settings) const;
    /** @endcond */
};

/** @cond */
template <Color From = Color::NIL, Color To = Color::NIL> class Convert {};
/** @endcond */

/**
 * colour to colour, no file involved: Convert<Color::RGB565, Color::RGBA8888>{}(image).
 * the heavy ones live behind Move.
 */
template <> class Convert<> {
public:
    /** @cond */
    constexpr Convert() = default;
    virtual ~Convert() = default;
    /** @endcond */

    /** @cond */
    // the same with both colours named at runtime. the pair that goes straight there
    // wins, and only a colour with no road of its own goes through rgba8888
    Result<AnyImage> operator()(AnyImage image, Color to) const;

    // bytes rather than an image, because at runtime there is no Image<P> to hand back:
    // the callers that want one already know which they want and say so to the pair
    Result<std::vector<uint8_t>> operator()(const Image<Color::RGBA8888>& image, Color to) const;
    /** @endcond */
};

/** @cond */
template <Color From = Color::NIL, Color To = Color::NIL> class Move {};
/** @endcond */

/**
 * the hot loop under Convert, and what a codec calls when it wants to write pixels
 * straight into a buffer it already owns: Move<Color::RGB565, Color::RGBA8888>{}(src,
 * dst, size). dst is sized by the caller so the simd paths can rely on it, and size is
 * here because block formats like etc1 cannot be moved pixel by pixel.
 *
 * it reports rather than assumes: the loop runs once per image, so one Result on the
 * way out costs nothing next to the work inside it.
 */
template <> class Move<> {
public:
    /** @cond */
    constexpr Move() = default;
    virtual ~Move() = default;
    /** @endcond */
};

/** can these bytes be recognised as this pair, asked of the pair rather than answered by hand */
template <Format F, Color P> concept Detectable = Util::MemberOf<Detect<F, P>, Detect<>>
    && requires (const Detect<F, P>& detect, Bytes d) { { detect(d) } -> std::same_as<bool>; };

/** can this pair be read into pixels, asked of the pair rather than answered by hand */
template <Format F, Color P> concept DecodesPlain = Util::MemberOf<Decode<F, P>, Decode<>>
    && requires (const Decode<F, P>& decode, CodecIn b) { { decode(b) } -> std::same_as<CodecRes<P>>; };
template <Format F, Color P> concept DecodesSettings = Util::MemberOf<Decode<F, P>, Decode<>>
    && requires (const Decode<F, P>& decode, CodecIn b, const Settings& s) { { decode(b, s) } -> std::same_as<CodecRes<P>>; };
template <Format F, Color P> concept Decodable = DecodesPlain<F, P> || DecodesSettings<F, P>;

/** can an image of this colour be written into this container, asked of the pair rather than answered by hand */
template <Format F, Color P> concept EncodesPlain = Util::MemberOf<Encode<F, P>, Encode<>>
    && requires (const Encode<F, P>& encode, Image<P> i) { { encode(std::move(i)) } -> std::same_as<CodecOut>; };
template <Format F, Color P> concept EncodesSettings = Util::MemberOf<Encode<F, P>, Encode<>>
    && requires (const Encode<F, P>& encode, Image<P> i, const Settings& s) { { encode(std::move(i), s) } -> std::same_as<CodecOut>; };
template <Format F, Color P> concept Encodable = EncodesPlain<F, P> || EncodesSettings<F, P>;

/** can this brush paint in this colour, asked of the pair rather than answered by hand */
template <Brush B, Color P> concept Generatable = Util::MemberOf<Generate<B, P>, Generate<>>
    && requires (const Generate<B, P>& generate, std::string_view w, const Settings& s) {
        { generate(w, s) } -> std::same_as<CodecRes<P>>;
    };

/** is there a loop from one colour to the other, asked of the pair rather than answered by hand */
template <Color From, Color To> concept Movable = Util::MemberOf<Move<From, To>, Move<>>
    && requires (const Move<From, To>& move, Bytes s, std::span<uint8_t> d, Size z) {
        { move(s, d, z) } -> std::same_as<Result<void>>;
    };

/** is there a whole image conversion from one colour to the other, asked of the pair rather than answered by hand */
template <Color From, Color To> concept Convertible = Util::MemberOf<Convert<From, To>, Convert<>>
    && requires (const Convert<From, To>& convert, Image<From> i) { { convert(std::move(i)) } -> std::same_as<CodecRes<To>>; };

/**
 * what a codec reads off the command line, declared in `include/haio/implementation/codecs/NAME.hpp`
 * beside nothing else. an option whose name has a colon in it, as png's
 * "png:compression-level" does, is reached through -define the way imagemagick does.
 */
struct Reads {
    std::span<const Option> decode = {};
    std::span<const Option> encode = {};
};

template <Format F> inline constexpr Reads reads = Reads{};

/**
 * what a brush takes on the command line, declared beside it in
 * include/haio/implementation/codecs/generators/: its options, what the words after its colon are,
 * and any other name it answers to, as xc does to canvas.
 */
struct Draws {
    std::span<const Option> options = {};
    std::string_view takes = {};
    std::array<std::string_view, 2> aliases = {};

    /**
     * a shape that, first in a -composite, is the shape rather than the background:
     * "qr:x xc:red xc:blue" is read as "qr:x -pix_fmt nil qr:x xc:red xc:blue", and
     * "qr:x xc:red" as "qr:x xc:red null:" before that
     */
    bool composite4 = false;
};

// "= Draws{}" for the same reason as reads above
template <Brush B> inline constexpr Draws draws = Draws{};

}

namespace Haio::Implementation::WIP {

/**
 * does this format name a colour of its own. a container that holds any of three has
 * nothing to declare, so this is asked rather than assumed.
 */
template <Format F> concept HasDefaultColor = requires { DefaultColor<F>::value; };

}

namespace Haio::Implementation {

/**
 * a decode or an encode with settings in hand, whichever form the codec wrote. one
 * that reads none is handed none: the command line has already refused any setting a
 * codec did not declare, so there is nothing here to drop on the floor.
 */
template <Format F, Color P>
    requires Codecs::Decodable<F, P>
Codecs::CodecRes<P> decode(Codecs::CodecIn blob, const Settings& settings) {
    if constexpr (Codecs::DecodesSettings<F, P>) return Codecs::Decode<F, P>{}(blob, settings);
    else return Codecs::Decode<F, P>{}(blob);
}

template <Format F, Color P>
    requires Codecs::Encodable<F, P>
Codecs::CodecOut encode(Image<P> image, const Settings& settings) {
    if constexpr (Codecs::EncodesSettings<F, P>) return Codecs::Encode<F, P>{}(std::move(image), settings);
    else return Codecs::Encode<F, P>{}(std::move(image));
}

}

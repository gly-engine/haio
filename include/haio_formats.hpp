#pragma once

#include <array>
#include <string_view>

namespace Haio {

/**
 * what a file is: the container that holds pixels. RAW means no container at all,
 * just the bytes of a colour format, which is why it needs its size supplied.
 *
 * the \@mime and \@ext tags are read by scripts/gen_scaffold.cpp, which builds the lookups both ways
 * from them. the first one is what haio writes, the rest are only accepted. this is what
 * a c++26 enum annotation will carry once the compiler parses them, and until then a
 * tag keeps them on the enumerator instead of in a table somewhere else.
 */
enum class Format {
    /** no mime type of its own: bare pixels are application/octet-stream */
    RAW,
    /**
     * @mime image/png
     * @ext png
     */
    PNG,
    /**
     * @mime image/x-portable-pixmap image/x-portable-anymap
     * @ext ppm pgm pbm pnm
     */
    PPM,
    /**
     * @mime image/jpeg
     * @ext jpg jpeg
     */
    JPEG,
    /**
     * @mime image/ktx
     * @ext ktx
     */
    KTX,
    /**
     * @mime image/ktx2
     * @ext ktx2
     */
    KTX2,
    /**
     * @mime image/vnd-ms.dds image/vnd.ms-dds
     * @ext dds
     */
    DDS,
    /**
     * @mime image/x-pvr
     * @ext pvr
     */
    PVR,
    /**
     * @mime image/x-zcis
     * @ext zcis
     */
    ZCIS,
    /**
     * @mime image/webp
     * @ext webp
     */
    WEBP,

    /**
     * not files but pictures drawn with escape codes, for looking at one without
     * leaving the terminal. they only ever encode: nothing arrives as ansi.
     *
     * @mime text/x-ansi
     * @ext ansi
     */
    ANSI,
    /**
     * @mime text/x-ansi-halfblock
     * @ext utf8
     */
    UTF8,
    /**
     * real pixels in a terminal that draws them, as foot, wezterm, mlterm or xterm
     * started as a vt340 do: a palette of colours and six rows at a time.
     *
     * @mime image/x-sixel
     * @ext sixel six
     */
    SIXEL,

    /**
     * a cartridge: a file with several things in it, only some of which are
     * pictures. which console it came from is read off the bytes and kept inside
     * the rom codecs, because nothing outside them has a use for the answer.
     *
     * the first mime is the one haio sends, and a cartridge is never written, so
     * it is only ever the label on an untouched file the cdn passed through.
     *
     * @mime application/x-nes-rom application/x-tic80-cart
     * @ext nes tic
     */
    ROM,

    /**
     * last on purpose. every format above starts with bytes that mean nothing else,
     * and a tga starts with its own image size: recognising one is a matter of the
     * header agreeing with itself, so it is asked only after everything that can
     * prove itself outright has said no.
     *
     * @mime image/x-tga image/x-targa image/tga
     * @ext tga targa tpic
     */
    TGA,

    /**
     * @mime image/gif
     * @ext gif
     */
    GIF,
};

/**
 * how the pixels are laid out. these never appear as a file on their own: the etc
 * family and RGB565 are what a gpu container carries, the rest are what codecs hand
 * around.
 */
enum class Color {
    RGBA8888,
    RGB888,
    RGB565,

    /**
     * five bits a channel packed into two bytes, little endian, with the top bit
     * spare: 0RRRRRGGGGGBBBBB. it is what a sixteen bit tga holds, and the spare bit
     * is why the same two bytes are a different colour once somebody declares it to
     * be alpha.
     */
    RGB555,

    /**
     * the same two bytes with the alpha at the other end: RRRRRGGGGGBBBBBA. the name
     * reads from the top bit down, which is the order the bits sit in the sixteen bit
     * word once it is read little endian.
     *
     * one bit of alpha is a yes or a no, so converting to it rounds: half opaque and
     * up survives, the rest goes clear.
     */
    RGBA5551,

    /**
     * blue first, which is how every windows era container stores a pixel. a tga, a
     * dds and a bmp all write their bytes this way round, so the swap belongs in one
     * colour rather than inside each of their decoders.
     */
    BGR888,
    BGRA8888,

    GRAY8,

    /** grey then alpha; to -compose tint a colour, where a GRAY8 is a mask */
    GRAYALPHA88,
    ETC1,

    /**
     * the same four by four blocks etc1 packs, with the modes etc2 added: planar for
     * a smooth gradient and t and h for the two colours etc1 could only average.
     *
     * an etc1 block is a valid etc2 block, so hardware that reads this reads both and
     * the two never need separate decoders. what they do not share is a name in
     * vulkan, which has no etc1 at all: a ktx2 asked for etc1 says etc2 in its header
     * and is read back as etc2, which decodes to the same pixels.
     */
    ETC2,

    /**
     * etc2 colour with an eac alpha block in front of it, so sixteen bytes a block
     * rather than eight. this is the one to ask for when the picture has holes in it:
     * every other etc is opaque, and a corner rounded away with -radius has nowhere
     * to go in them.
     */
    ETC2A,

    /**
     * one eleven bit channel a block, which is what a height map or the x of a normal
     * map wants: eight bytes for sixteen pixels with nothing spent on colour. it is
     * written from the red channel and read back into it, so the other two come back
     * black rather than grey -- this is one channel and says so.
     */
    EAC_R11,

    /** two of those side by side, sixteen bytes a block, for a two channel normal map */
    EAC_RG11,

    YUV420,

    /**
     * an index per pixel plus the colours it indexes, which is why this is the one
     * colour whose Image carries something besides pixels.
     */
    PALETTE,

    /**
     * how a console keeps its indices, which is not one per byte.
     *
     * nes chr is two bits a pixel, in eight by eight tiles, with the two bits of a
     * pixel living in separate planes eight bytes apart. that is a layout and not a
     * container, the same way ETC1 is, so it is a colour: a .chr file is Format::RAW
     * holding this, and converting to PALETTE is what unpacks it.
     */
    CHR_NES,

    /**
     * a size and no pixels. every colour becomes it by forgetting its pixels, and it
     * becomes rgba8888 as a transparent picture, so "-pix_fmt nil" is a clear canvas
     * as big as the picture it was, with nothing allocated until something draws.
     */
    NIL,
};

/**
 * the other names these layouts go by.
 *
 * ffmpeg's -pix_fmt is where most people learned to name a pixel layout on a command
 * line, so what they already type for it is accepted beside the enumerator's own
 * name. the list sits next to the enum rather than inside the lookup that reads it,
 * because the command line grammar prints it too, and a list written in two places is
 * a list that drifts.
 */
struct ColorAlias {
    std::string_view spelling;
    Color color;
};

inline constexpr ColorAlias colorAliases[] = {
    // short spellings the enumerator name cannot carry until enum annotations land
    {"rgb", Color::RGB888},
    {"rgba", Color::RGBA8888},
    {"rgb24", Color::RGB888},
    {"bgr24", Color::BGR888},
    {"bgra", Color::BGRA8888},
    // the way it tends to get written down when it is listed beside rgba8888
    {"bgr8888", Color::BGRA8888},
    {"rgb565le", Color::RGB565},
    {"rgb555le", Color::RGB555},
    {"gray", Color::GRAY8},
    {"grey", Color::GRAY8},
    {"g8", Color::GRAY8},
    // imagemagick's -type, for the greys
    {"grayscale", Color::GRAY8},
    {"greyscale", Color::GRAY8},
    {"grayscalealpha", Color::GRAYALPHA88},
    {"greyscalealpha", Color::GRAYALPHA88},
    {"ya8", Color::GRAYALPHA88},
    {"gray8a", Color::GRAYALPHA88},
    {"pal8", Color::PALETTE},
    // the long spellings, for whoever writes the gpu format out in full
    {"etc2_rgb", Color::ETC2},
    {"etc2rgb", Color::ETC2},
    {"etc2_rgba", Color::ETC2A},
    {"etc2rgba", Color::ETC2A},
    {"etc2_eac", Color::ETC2A},
    // and the order khronos names them in, which is the other way round
    {"r11_eac", Color::EAC_R11},
    {"rg11_eac", Color::EAC_RG11},
    {"eac_r", Color::EAC_R11},
    {"eac_rg", Color::EAC_RG11},
    // the p is for planar, which is the only way haio keeps it
    {"yuv420p", Color::YUV420},
    {"null", Color::NIL},
};


/**
 * what paints a picture out of nothing, the way a Format reads one out of bytes:
 * "xc:white", "gradient:red-blue". imagemagick calls these generators and files them
 * with its coders; they are kept apart here because they have no bytes to recognise
 * and nothing to write, and making them a Format meant pretending otherwise.
 *
 * the name on the command line is the enumerator in lower case with a dash between
 * its words, so RadialGradient is radial-gradient. any other name a brush answers to
 * is declared with it, in include/haio/codecs/generators/.
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

}

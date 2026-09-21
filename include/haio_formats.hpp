#pragma once

#include <array>
#include <string_view>

namespace Haio {

/**
 * what a file is: the container that holds pixels. RAW means no container at all,
 * just the bytes of a colour format, which is why it needs its size supplied.
 *
 * the \@mime and \@ext tags are read by scripts/gen_codecs.cpp, which builds the lookups both ways
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
     * a cartridge, which is a container like any other: it says where its pattern
     * data begins and how much of it there is.
     *
     * @mime application/x-nes-rom
     * @ext nes
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
};

/**
 * how the pixels are laid out. these never appear as a file on their own: ETC1 and
 * RGB565 are what a gpu container carries, the rest are what codecs hand around.
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
    ETC1,
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
    {"pal8", Color::PALETTE},
    // the p is for planar, which is the only way haio keeps it
    {"yuv420p", Color::YUV420},
};

}

#pragma once

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
};

/**
 * how the pixels are laid out. these never appear as a file on their own: ETC1 and
 * RGB565 are what a gpu container carries, the rest are what codecs hand around.
 */
enum class Color {
    RGBA8888,
    RGB888,
    RGB565,
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

}

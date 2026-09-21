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
};

}

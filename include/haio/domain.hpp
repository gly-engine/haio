#pragma once

#include <array>
#include <string_view>

namespace Haio {

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
     * @mime application/x-nes-rom
     * @ext nes
     */
    ROM,

    /**
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

enum class Color {
    /** @short True Color */
    RGBA8888,
    /** @short Opaque True Color */
    RGB888,
    /** @short High Color */
    RGB565,
    /** @short High Color 15 Bit */
    RGB555,
    /** @short High Color with 1 Bit Alpha */
    RGBA5551,
    /** @short Opaque True Color, Blue First */
    BGR888,
    /** @short True Color, Blue First */
    BGRA8888,
    /** @short Gray Scale */
    GRAY8,
    /** @short Gray Scale with Alpha */
    GRAYALPHA88,
    /** @short Lossy Texture Compression */
    ETC1,
    /** @short Chroma Subsampled */
    YUV420,
    /** @short Indexed Color */
    PALETTE,
    /** @short NES Tile Pattern */
    CHR_NES,
    /** @short Size Only */
    NIL,
};

namespace Implementation::Trash {

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
    // the p is for planar, which is the only way haio keeps it
    {"yuv420p", Color::YUV420},
    {"null", Color::NIL},
};

}

}

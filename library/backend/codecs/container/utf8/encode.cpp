#include <haio_codec.hpp>
#include <haio_codecs.hpp>
#include <haio_convert.hpp>

#include <string>

namespace {

/** U+2580, which fills the top half of the cell and leaves the bottom to the background */
constexpr std::string_view halfBlock = "▀";

void appendNumber(std::string& out, int value) {
    out += std::to_string(value);
}

}

namespace Haio::Codecs {

/**
 * two pixel rows per terminal row, using the upper half block.
 *
 * the foreground paints the top pixel and the background the bottom one, which buys
 * twice the vertical resolution of the ansi rendering for the same number of lines
 * and makes the aspect ratio come out roughly square.
 *
 * an odd number of rows leaves the last cell with nothing below it, and its
 * background is left black rather than repeating the row above: a doubled last line
 * reads as part of the picture, a dark one reads as the edge.
 */
template <>
Result<Blob> Encode<Format::UTF8, Color::RGB888>(Image<Color::RGB888> img) {
    const Size size{img.width, img.height};
    HAIO_TRY(expected, sizeOf(Color::RGB888, size));
    if (img.data.size() != expected) HAIO_FAIL(InvalidInput, "invalid rgb888 image for utf8 encode");

    const auto width = static_cast<size_t>(img.width);
    const auto height = static_cast<size_t>(img.height);

    std::string out;
    out.reserve(width * ((height + 1) / 2) * 40);

    const auto appendColour = [&](std::string_view lead, size_t row, size_t x, bool present) {
        out += lead;
        if (!present) {
            out += "0;0;0";
            return;
        }
        const auto at = (row * width + x) * 3;
        appendNumber(out, img.data[at + 0]);
        out += ';';
        appendNumber(out, img.data[at + 1]);
        out += ';';
        appendNumber(out, img.data[at + 2]);
    };

    for (size_t y = 0; y < height; y += 2) {
        for (size_t x = 0; x < width; x++) {
            appendColour("\x1b[38;2;", y, x, true);
            appendColour(";48;2;", y + 1, x, y + 1 < height);
            out += 'm';
            out += halfBlock;
        }
        out += "\x1b[0m\n";
    }

    return Blob{Format::UTF8, Color::RGB888, "text/x-ansi-halfblock", {},
                std::vector<uint8_t>(out.begin(), out.end())};
}

}

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

    const auto colourAt = [&](size_t row, size_t x, bool present) -> uint32_t {
        if (!present) return 0;
        const auto at = (row * width + x) * 3;
        return (static_cast<uint32_t>(img.data[at + 0]) << 16)
             | (static_cast<uint32_t>(img.data[at + 1]) << 8)
             | img.data[at + 2];
    };

    const auto appendColour = [&](std::string_view lead, uint32_t colour) {
        out += lead;
        appendNumber(out, static_cast<int>((colour >> 16) & 0xFF));
        out += ';';
        appendNumber(out, static_cast<int>((colour >> 8) & 0xFF));
        out += ';';
        appendNumber(out, static_cast<int>(colour & 0xFF));
    };

    for (size_t y = 0; y < height; y += 2) {
        /**
         * a terminal keeps the colour it was last told, so saying it again is bytes
         * nobody reads. a picture in few colours repeats itself constantly, and this
         * is exactly the picture somebody renders in a terminal.
         *
         * the pair is tracked rather than each half, because a cell sets both at once.
         */
        uint32_t lastTop = 0;
        uint32_t lastBottom = 0;
        bool anySoFar = false;

        for (size_t x = 0; x < width; x++) {
            const auto top = colourAt(y, x, true);
            const auto bottom = colourAt(y + 1, x, y + 1 < height);

            if (!anySoFar || top != lastTop || bottom != lastBottom) {
                appendColour("\x1b[38;2;", top);
                appendColour(";48;2;", bottom);
                out += 'm';
                lastTop = top;
                lastBottom = bottom;
                anySoFar = true;
            }
            out += halfBlock;
        }
        // the reset ends the row, so the next one starts with nothing assumed
        out += "\x1b[0m\n";
    }

    return Blob{Format::UTF8, Color::RGB888, "text/x-ansi-halfblock", {},
                std::vector<uint8_t>(out.begin(), out.end())};
}

}

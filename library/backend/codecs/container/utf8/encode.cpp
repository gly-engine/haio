#include <haio_codec.hpp>
#include <haio/generated/codec.hpp>
#include <haio_convert.hpp>

#include <string>

namespace {

/** U+2580 and U+2584, which fill one half of the cell and leave the other to the background */
constexpr std::string_view upperHalf = "▀";
constexpr std::string_view lowerHalf = "▄";

void appendNumber(std::string& out, int value) {
    out += std::to_string(value);
}

}

namespace Haio::Codecs {

/**
 * two pixel rows per terminal row, using the half blocks.
 *
 * the foreground paints the top pixel and the background the bottom one, which buys
 * twice the vertical resolution of the ansi rendering for the same number of lines
 * and makes the aspect ratio come out roughly square.
 *
 * a terminal cannot blend, so a pixel under half opaque is not there, and where it is
 * not the terminal's own background shows: a clear top is the lower half block in the
 * bottom's colour, and a clear cell is a space. an odd number of rows leaves the last
 * cell with nothing below it, which is clear the same way.
 */
template <>
Result<Blob> Encode<Format::UTF8, Color::RGBA8888>(Image<Color::RGBA8888> img) {
    const Size size{img.width, img.height};
    HAIO_TRY(expected, sizeOf(Color::RGBA8888, size));
    if (img.data.size() != expected) HAIO_FAIL(InvalidInput, "invalid rgba8888 image for utf8 encode");

    const auto width = static_cast<size_t>(img.width);
    const auto height = static_cast<size_t>(img.height);

    std::string out;
    out.reserve(width * ((height + 1) / 2) * 40);

    const auto pixelAt = [&](size_t row, size_t x) -> const uint8_t* {
        if (row >= height) return nullptr;
        const auto* pixel = img.data.data() + (row * width + x) * 4;
        return pixel[3] >= 0x80 ? pixel : nullptr;
    };

    const auto appendColour = [&](std::string& into, std::string_view lead, const uint8_t* pixel) {
        into += lead;
        appendNumber(into, pixel[0]);
        into += ';';
        appendNumber(into, pixel[1]);
        into += ';';
        appendNumber(into, pixel[2]);
    };

    std::string colours;
    for (size_t y = 0; y < height; y += 2) {
        /**
         * a terminal keeps the colour it was last told, so saying it again is bytes
         * nobody reads. a picture in few colours repeats itself constantly, and this
         * is exactly the picture somebody renders in a terminal.
         *
         * the whole escape is compared rather than each half, because a cell sets both at once.
         */
        std::string last;

        for (size_t x = 0; x < width; x++) {
            const auto* top = pixelAt(y, x);
            const auto* bottom = pixelAt(y + 1, x);

            colours.clear();
            std::string_view glyph = " ";
            if (top && bottom) {
                appendColour(colours, "\x1b[38;2;", top);
                appendColour(colours, ";48;2;", bottom);
                glyph = upperHalf;
            } else if (top || bottom) {
                appendColour(colours, "\x1b[38;2;", top ? top : bottom);
                colours += ";49";
                glyph = top ? upperHalf : lowerHalf;
            } else {
                colours += "\x1b[49";
            }
            colours += 'm';

            if (colours != last) {
                out += colours;
                last = colours;
            }
            out += glyph;
        }
        // the reset ends the row, so the next one starts with nothing assumed
        out += "\x1b[0m\n";
    }

    return Blob{Format::UTF8, Color::RGBA8888, "text/x-ansi-halfblock", {},
                std::vector<uint8_t>(out.begin(), out.end())};
}

}

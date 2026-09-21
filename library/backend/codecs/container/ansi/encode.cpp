#include <haio_codec.hpp>
#include <haio_codecs.hpp>
#include <haio_convert.hpp>

#include <string>

namespace {

/** a cell is two spaces because a terminal cell is about twice as tall as it is wide */
constexpr std::string_view cell = "  ";

void appendNumber(std::string& out, int value) {
    out += std::to_string(value);
}

}

namespace Haio::Codecs {

/**
 * one terminal cell per pixel, coloured by its background.
 *
 * the picture ends up half the height of the utf8 one for the same number of rows,
 * and it works on anything that can colour a background, including terminals with no
 * font to draw a half block with.
 *
 * @todo a terminal that cannot do 24 bit colour renders this as noise. detecting that
 * means reading COLORTERM, and falling back means quantising to the 256 colour cube.
 */
template <>
Result<Blob> Encode<Format::ANSI, Color::RGB888>(Image<Color::RGB888> img) {
    const Size size{img.width, img.height};
    HAIO_TRY(expected, sizeOf(Color::RGB888, size));
    if (img.data.size() != expected) HAIO_FAIL(InvalidInput, "invalid rgb888 image for ansi encode");

    std::string out;
    // every cell carries an escape sequence of its own, so the text is far larger
    // than the picture: reserving up from the start saves growing it a hundred times
    out.reserve(static_cast<size_t>(img.width) * static_cast<size_t>(img.height) * 20);

    for (int y = 0; y < img.height; y++) {
        // a terminal keeps the colour it was last told, and a run of one colour is
        // the common case in the pictures anybody renders here
        uint32_t last = 0;
        bool anySoFar = false;

        for (int x = 0; x < img.width; x++) {
            const auto at = (static_cast<size_t>(y) * static_cast<size_t>(img.width) + static_cast<size_t>(x)) * 3;
            const uint32_t colour = (static_cast<uint32_t>(img.data[at + 0]) << 16)
                                  | (static_cast<uint32_t>(img.data[at + 1]) << 8)
                                  | img.data[at + 2];

            if (!anySoFar || colour != last) {
                out += "\x1b[48;2;";
                appendNumber(out, img.data[at + 0]);
                out += ';';
                appendNumber(out, img.data[at + 1]);
                out += ';';
                appendNumber(out, img.data[at + 2]);
                out += 'm';
                last = colour;
                anySoFar = true;
            }
            out += cell;
        }
        // reset at the end of every row, so a terminal that wraps does not paint the
        // rest of the line in whatever colour the last pixel was
        out += "\x1b[0m\n";
    }

    return Blob{Format::ANSI, Color::RGB888, "text/x-ansi", {}, std::vector<uint8_t>(out.begin(), out.end())};
}

}

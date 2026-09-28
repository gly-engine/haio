#pragma once

#include "haio_codec.hpp"
#include "haio/internal/codecs/canvas.hpp"

#include <zint.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <string>

/**
 * what code: and qr: share: zint encodes, and the modules it lays out are painted
 * here the way every brush paints, rather than through zint's own rendering, which is
 * left out of the build along with its file writers.
 */
namespace Haio::Codecs::Zint {

/**
 * one symbol, encoded and painted, with a margin of so many modules around it and no
 * more: the picture is the code and its margin, a module a pixel when nobody says
 * -size.
 *
 * with -size the code is scaled by the largest whole number of pixels a module can be
 * and still fit, and centred: a module is never a pixel wide in one place and two in
 * another, which is what stretching it to fit would do and what a scanner minds most.
 * a linear code fills the height it is given.
 *
 * a hole, when there is one, is left in the middle in the background colour: the
 * pixels asked for, grown to whole modules and centred on the module grid. without
 * -size the hole is what decides the scale, so the picture comes out as small as it
 * can be with that hole in it and the code still readable.
 */
template <typename Tweak>
Result<Image<Color::RGBA8888>> paint(int symbology, std::string_view words, const Settings& settings, Tweak&& tweak,
                                     int margin = 0, std::optional<Size> hole = std::nullopt) {
    if (words.empty()) HAIO_FAIL(InvalidInput, "nothing to encode; it goes after the colon, as in qr:hello");

    const std::unique_ptr<zint_symbol, decltype(&ZBarcode_Delete)> symbol(ZBarcode_Create(), ZBarcode_Delete);
    if (!symbol) HAIO_FAIL(Internal, "zint could not start");
    symbol->symbology = symbology;
    // the words arrive as utf-8, and zint converts from that to whatever the code holds
    symbol->input_mode = UNICODE_MODE;
    if (auto refused = tweak(*symbol)) return std::unexpected(*refused);

    const auto encoded = ZBarcode_Encode(symbol.get(), reinterpret_cast<const unsigned char*>(words.data()),
                                         static_cast<int>(words.size()));
    if (encoded >= ZINT_ERROR) {
        HAIO_FAIL(InvalidInput, "unable to encode " + Stages::quoted(words) + ": " + std::string(symbol->errtxt));
    }

    HAIO_TRY(ink, Canvas::colourSetting(settings, Canvas::fill));
    HAIO_TRY(paper, Canvas::colourSetting(settings, Canvas::background));

    const bool linear = symbol->rows == 1;
    const int columns = symbol->width;
    const int rows = linear ? std::max(1, static_cast<int>(std::lround(symbol->height))) : symbol->rows;

    // the margin is counted with the code, so a -size has room for both
    const int wide = columns + 2 * margin;
    const int tall = rows + 2 * margin;

    /**
     * the hole in modules at a given scale: the pixels asked for rounded up to whole
     * modules, and one more where that leaves it off centre, so it sits on the grid the
     * way the code does. it has to stay clear of the finder patterns in the corners --
     * seven modules and a separator -- and be no more than a third of the code each
     * way. a reader was measured losing the code somewhere between 38% and 47% of the
     * side, depending on the version, so a third is that with room to spare.
     */
    struct Hole {
        int left = 0, top = 0, wide = 0, tall = 0;
    };
    const auto holeAt = [&](int scale) {
        Hole out{0, 0, (hole->width + scale - 1) / scale, (hole->height + scale - 1) / scale};
        if ((columns - out.wide) % 2) out.wide++;
        if ((rows - out.tall) % 2) out.tall++;
        out.left = (columns - out.wide) / 2;
        out.top = (rows - out.tall) / 2;
        return out;
    };
    const auto readable = [&](const Hole& h) {
        constexpr int finder = 8;
        return h.left >= finder && h.top >= finder && h.wide * 3 <= columns && h.tall * 3 <= rows;
    };

    int scale = 1;
    Size size{wide, tall};
    if (settingNamed(settings, Canvas::size.name())) {
        HAIO_TRY(wanted, Canvas::sizeOf(settings));
        scale = linear ? wanted.width / wide : std::min(wanted.width / wide, wanted.height / tall);
        if (scale < 1) {
            HAIO_FAIL(InvalidInput, "this code is " + std::to_string(wide) + "x" + std::to_string(tall)
                                        + " modules with its margin, more than fits in "
                                        + Stages::quoted(std::to_string(wanted.width) + "x" + std::to_string(wanted.height)));
        }
        size = wanted;
    } else if (hole) {
        /**
         * no -size, so the hole says how big the picture has to be: the fewest pixels a
         * module that leave room for it. a larger scale only ever shrinks the hole in
         * modules, so the first one that reads is the smallest picture that does.
         */
        while (!readable(holeAt(scale)) && scale < Canvas::largest / std::max(wide, tall)) scale++;
        size = Size{wide * scale, tall * scale};
    }

    Hole cleared;
    if (hole) {
        cleared = holeAt(scale);
        if (!readable(cleared)) {
            HAIO_FAIL(InvalidInput, "a " + std::to_string(cleared.wide) + "x" + std::to_string(cleared.tall)
                                        + " module hole in a " + std::to_string(columns) + "x" + std::to_string(rows)
                                        + " module code would leave it unreadable "
                                        + Stages::quoted(std::to_string(hole->width) + "x" + std::to_string(hole->height)));
        }
    }
    const auto inHole = [&](int x, int y) {
        return hole && x >= cleared.left && x < cleared.left + cleared.wide && y >= cleared.top
            && y < cleared.top + cleared.tall;
    };

    // a linear code has one row of modules, stretched as tall as it is drawn
    const auto dark = [&](int x, int y) {
        if (inHole(x, y)) return false;
        const int row = linear ? 0 : y;
        return ((symbol->encoded_data[row][x >> 3] >> (x & 7)) & 1) != 0;
    };

    auto image = Canvas::blank(size);
    for (int y = 0; y < size.height; y++) {
        for (int x = 0; x < size.width; x++) Canvas::put(image, x, y, paper);
    }

    const int left = (size.width - wide * scale) / 2 + margin * scale;
    const int top = linear ? 0 : (size.height - tall * scale) / 2 + margin * scale;
    const int height = linear ? size.height : rows * scale;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < columns * scale; x++) {
            if (dark(x / scale, linear ? 0 : y / scale)) Canvas::put(image, left + x, top + y, ink);
        }
    }
    return image;
}

}

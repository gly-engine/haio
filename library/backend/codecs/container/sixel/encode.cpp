#include <haio_codec.hpp>
#include <haio_convert.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace {

/**
 * the colours a picture with no palette of its own is drawn in: six steps of red and
 * blue and seven of green, which the eye tells apart best. fixed rather than picked
 * from the picture, since haio does not choose colours for anybody; -palete does.
 */
constexpr int reds = 6;
constexpr int greens = 7;
constexpr int blues = 6;

int step(uint8_t value, int steps) {
    return (value * (steps - 1) + 127) / 255;
}

int level(int step, int steps) {
    return step * 255 / (steps - 1);
}

/** a register's colour, which sixel writes in percent */
void defineColour(std::string& out, size_t index, uint32_t argb) {
    const auto percent = [](uint32_t channel) { return std::to_string((channel * 100 + 127) / 255); };
    out += '#' + std::to_string(index) + ";2;" + percent((argb >> 16) & 0xFF) + ';' + percent((argb >> 8) & 0xFF) + ';'
         + percent(argb & 0xFF);
}

/** a run of one sixel, written out when short and counted with ! when longer */
void appendRun(std::string& out, char sixel, int count) {
    if (count > 3) {
        out += '!' + std::to_string(count) + sixel;
    } else {
        out.append(static_cast<size_t>(count), sixel);
    }
}

/**
 * the picture as indices into colours, six rows at a time: each band is painted once
 * per colour that appears in it, every column a character whose six bits say which of
 * its rows take that colour, $ going back to the start of the band and - down to the
 * next. a pixel with no index is left alone, so the terminal's background shows.
 */
template <typename IndexAt>
std::string draw(int width, int height, const std::vector<uint32_t>& colours, IndexAt indexAt) {
    std::string out = "\x1bP0;1;0q\"1;1;" + std::to_string(width) + ';' + std::to_string(height);

    std::vector<bool> present(colours.size());
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (const auto index = indexAt(x, y); index >= 0) present[static_cast<size_t>(index)] = true;
        }
    }
    for (size_t at = 0; at < colours.size(); at++) {
        if (present[at]) defineColour(out, at, colours[at]);
    }

    std::vector<std::vector<uint8_t>> bands(colours.size());
    std::vector<size_t> used;
    for (int top = 0; top < height; top += 6) {
        used.clear();
        for (int row = 0; row < 6 && top + row < height; row++) {
            for (int x = 0; x < width; x++) {
                const auto index = indexAt(x, top + row);
                if (index < 0) continue;
                auto& band = bands[static_cast<size_t>(index)];
                if (band.empty()) {
                    band.assign(static_cast<size_t>(width), 0);
                    used.push_back(static_cast<size_t>(index));
                }
                band[static_cast<size_t>(x)] |= static_cast<uint8_t>(1u << row);
            }
        }

        for (size_t at = 0; at < used.size(); at++) {
            if (at > 0) out += '$';
            out += '#' + std::to_string(used[at]);

            auto& band = bands[used[at]];
            for (int x = 0; x < width;) {
                const auto bits = band[static_cast<size_t>(x)];
                int count = 1;
                while (x + count < width && band[static_cast<size_t>(x + count)] == bits) count++;
                appendRun(out, static_cast<char>('?' + bits), count);
                x += count;
            }
            band.clear();
        }
        out += '-';
    }
    out += "\x1b\\";
    return out;
}

Haio::Blob blobOf(std::string text) {
    return Haio::Blob{Haio::Format::SIXEL, Haio::Color::RGBA8888, "image/x-sixel", {}, {text.begin(), text.end()}};
}

}

namespace Haio::Codecs {

template <> struct DefaultColor<Format::SIXEL> { static constexpr Color value = Color::RGBA8888; };

/**
 * @addtogroup encode
 * @{
 */

/** full colour, drawn in the fixed colours above; a pixel under half opaque is not drawn */
template <>
Result<Blob> Encode<Format::SIXEL, Color::RGBA8888>(Image<Color::RGBA8888> img) {
    HAIO_TRY(expected, sizeOf(Color::RGBA8888, Size{img.width, img.height}));
    if (img.width <= 0 || img.data.size() != expected) HAIO_FAIL(InvalidInput, "invalid rgba8888 image for sixel encode");

    std::vector<uint32_t> colours;
    colours.reserve(reds * greens * blues);
    for (int r = 0; r < reds; r++) {
        for (int g = 0; g < greens; g++) {
            for (int b = 0; b < blues; b++) {
                colours.push_back(0xFF000000u | static_cast<uint32_t>(level(r, reds)) << 16
                                  | static_cast<uint32_t>(level(g, greens)) << 8 | static_cast<uint32_t>(level(b, blues)));
            }
        }
    }

    return blobOf(draw(img.width, img.height, colours, [&](int x, int y) {
        const auto at = (static_cast<size_t>(y) * static_cast<size_t>(img.width) + static_cast<size_t>(x)) * 4;
        if (img.data[at + 3] < 128) return -1;
        return (step(img.data[at], reds) * greens + step(img.data[at + 1], greens)) * blues + step(img.data[at + 2], blues);
    }));
}

/** a picture already fitted to a palette, drawn in exactly its colours */
template <>
Result<Blob> Encode<Format::SIXEL, Color::PALETTE>(Image<Color::PALETTE> img) {
    if (img.width <= 0 || img.data.size() != static_cast<size_t>(img.width) * static_cast<size_t>(img.height)) {
        HAIO_FAIL(InvalidInput, "invalid palette image for sixel encode");
    }
    for (const auto index : img.data) {
        if (index >= img.entries.size()) HAIO_FAIL(InvalidInput, "a palette index past the colours it has");
    }

    return blobOf(draw(img.width, img.height, img.entries, [&](int x, int y) {
        return static_cast<int>(img.data[static_cast<size_t>(y) * static_cast<size_t>(img.width) + static_cast<size_t>(x)]);
    }));
}

/** @} */

}

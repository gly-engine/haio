#include <haio/internal/codecs/yuv.hpp>
#include <haio/internal/codecs/zcis.hpp>

#include <cstdint>

namespace Haio::Codecs::Zcis {

namespace {

/**
 * where colour stops being worth giving up, in the sixteenths of a level the slack
 * is counted in.
 *
 * a run joins its neighbour when every channel is within the slack, and by thirty
 * two levels a photograph has become a poster: the draws have long since stopped
 * falling and only the picture is still going. a number that cannot be reached by
 * here cannot be reached, so this is where the search gives up rather than ruins.
 */
constexpr int ceiling = 32 * 16;

/**
 * what the picture costs to put up: one draw for every run of one colour in every
 * row of every rectangle that travels.
 *
 * the width of a run does not appear because it does not cost anything -- a
 * television draws one pixel and a hundred for the same price, which is the whole
 * reason any of this exists.
 */
size_t drawsOf(const std::vector<Rect>& boxes, auto&& keyAt) {
    size_t count = 0;
    for (const auto& box : boxes) {
        if (box.width <= 0 || box.height <= 0) continue;
        for (int y = box.y; y < box.y + box.height; y++) {
            auto run = keyAt(box.x, y);
            count++;
            for (int x = box.x + 1; x < box.x + box.width; x++) {
                const auto key = keyAt(x, y);
                if (key == run) continue;
                run = key;
                count++;
            }
        }
    }
    return count;
}

/**
 * the least colour that buys what was asked for.
 *
 * nought is tried on its own first because it is the answer whenever the flat
 * ground alone is enough, and the flat ground costs no colour at all: a grid of
 * thumbnails on white can lose a third of its drawing without a pixel moving.
 *
 * the rest is a halving search, which assumes that more slack never costs more
 * draws. a greedy run can in principle break that by an odd run here or there, and
 * the price of it breaking is a sixteenth of a level more than the least -- small
 * enough next to the five hundred passes it saves over walking up one at a time.
 */
int least(size_t target, auto&& drawsAt) {
    if (drawsAt(0) <= target) return 0;

    int low = 1;
    int high = ceiling;
    while (low < high) {
        const auto middle = low + (high - low) / 2;
        if (drawsAt(middle) <= target) {
            high = middle;
        } else {
            low = middle + 1;
        }
    }
    return low;
}

/** the colour of a pixel, as the television compares them */
auto keyOf(const Image<Color::RGBA8888>& img) {
    return [data = img.data.data(), stride = static_cast<size_t>(img.width) * 4](int x, int y) {
        const auto at = static_cast<size_t>(y) * stride + static_cast<size_t>(x) * 4;
        return (static_cast<uint32_t>(data[at + 3]) << 24) | (static_cast<uint32_t>(data[at]) << 16)
             | (static_cast<uint32_t>(data[at + 1]) << 8) | data[at + 2];
    };
}

/** the same, for a frame whose chroma is shared by every two by two block */
auto keyOf(const Image<Color::YUV420>& img) {
    const auto planes = Yuv420::planesOf(Size{img.width, img.height});
    const auto* luma = img.data.data();
    const auto* blue = luma + planes.luma;
    const auto* red = blue + planes.chroma;

    return [=](int x, int y) {
        const auto at = static_cast<size_t>(y) * planes.lumaWidth + static_cast<size_t>(x);
        const auto chroma = static_cast<size_t>(y / 2) * planes.chromaWidth + static_cast<size_t>(x / 2);
        return (static_cast<uint32_t>(luma[at]) << 16) | (static_cast<uint32_t>(blue[chroma]) << 8) | red[chroma];
    };
}

/**
 * the ground is found before the colour is given up, and not after.
 *
 * run extension anchors each row on its own first pixel, so a background that was
 * one colour everywhere comes out of it a shade different in every row -- and a
 * background that is no longer one colour has no ground to name, so the box grows
 * back to the whole cut and the file gets bigger the harder it is squeezed. finding
 * the ground first costs nothing and keeps the two from fighting.
 */

/**
 * the share of the draws the number asks to be rid of, counted against the picture
 * as it would travel with nothing done to it at all.
 *
 * the plain picture is the mark rather than the picture at this level of
 * optimization, and that is the point of the whole thing: every saving the encoder
 * finds without touching colour counts towards the number, so a file that gains the
 * fill command needs less slack to reach the same fifty than it did before.
 */
size_t targetOf(size_t plain, int wanted) {
    return plain - plain * static_cast<size_t>(wanted) / 100;
}

}

int minify(const Image<Color::RGBA8888>& img, const std::vector<Rect>& cuts, size_t worth, int level, int wanted) {
    if (wanted <= 0 || cuts.empty()) return 0;

    const auto target = targetOf(drawsOf(cuts, keyOf(img)), wanted);
    return least(target, [&](int tolerance) {
        const auto flat = level >= 1 ? flatten(img, cuts, worth, tolerance / 16) : Flat{};
        auto copy = img;
        lengthen(copy, tolerance);
        if (level < 1) return drawsOf(cuts, keyOf(copy));
        return drawsOf(flat.boxes, keyOf(copy)) + flat.fills.size();
    });
}

int minifyYuv(const Image<Color::YUV420>& img, const std::vector<Rect>& cuts, size_t worth, bool even, int level,
              int wanted) {
    if (wanted <= 0 || cuts.empty()) return 0;

    const auto target = targetOf(drawsOf(cuts, keyOf(img)), wanted);
    return least(target, [&](int tolerance) {
        const auto flat = level >= 1 ? flattenYuv(img, cuts, worth, even, tolerance / 16) : Flat{};
        auto copy = img;
        lengthen(copy, tolerance);
        if (level < 1) return drawsOf(cuts, keyOf(copy));
        return drawsOf(flat.boxes, keyOf(copy)) + flat.fills.size();
    });
}

}

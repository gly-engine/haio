#include <haio/internal/codecs/yuv.hpp>
#include <haio/internal/codecs/zcis.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_map>

namespace Haio::Codecs::Zcis {

namespace {

/**
 * the most common colour in a rectangle, and how much of it there is.
 *
 * the whole rectangle is counted rather than its border because a border is a
 * guess: a thumbnail with a shadow or a rounded corner has a border that is not
 * the background, and a picture of the sky has a background that is nowhere near
 * its border. counting is cheap next to what is saved.
 */
template <typename Key>
std::pair<Key, size_t> commonest(const Rect& at, const Key invalid, auto&& keyAt) {
    std::unordered_map<Key, size_t> seen;
    for (int y = at.y; y < at.y + at.height; y++) {
        for (int x = at.x; x < at.x + at.width; x++) seen[keyAt(x, y)]++;
    }

    Key best = invalid;
    size_t most = 0;
    for (const auto& [key, count] : seen) {
        if (count > most) {
            most = count;
            best = key;
        }
    }
    return {best, most};
}

/** the three bytes of a key, far enough apart to matter */
bool apart(uint32_t left, uint32_t right, int tolerance) {
    for (int shift = 0; shift <= 16; shift += 8) {
        const auto a = static_cast<int>((left >> shift) & 0xFF);
        const auto b = static_cast<int>((right >> shift) & 0xFF);
        if (a - b > tolerance || b - a > tolerance) return true;
    }
    return false;
}

/**
 * everything that is not the background, boxed.
 *
 * with slack the box is smaller and the pixels it leaves behind are painted the
 * background rather than themselves, which is the whole of what tolerance costs.
 */
template <typename Key>
Rect boxOf(const Rect& at, const Key background, int tolerance, auto&& keyAt) {
    int left = at.x + at.width;
    int top = at.y + at.height;
    int right = at.x - 1;
    int bottom = at.y - 1;

    for (int y = at.y; y < at.y + at.height; y++) {
        for (int x = at.x; x < at.x + at.width; x++) {
            if (!apart(keyAt(x, y), background, tolerance)) continue;
            left = std::min(left, x);
            right = std::max(right, x);
            top = std::min(top, y);
            bottom = std::max(bottom, y);
        }
    }

    if (right < left) return Rect{at.x, at.y, 0, 0};  // the whole cut is the background
    return Rect{left, top, right - left + 1, bottom - top + 1};
}

/**
 * outwards to even edges, which is the least a 4:2:0 frame can hold, and nothing
 * if that no longer fits inside the cut.
 *
 * a box that leaves its cut would paint over the neighbour below or to the right,
 * and the neighbour has no reason to paint that pixel back: a cut with an odd side
 * has a last row no even box can reach, so the answer there is to send it whole.
 */
std::optional<Rect> evenly(const Rect& box, const Rect& bounds) {
    if (box.width == 0) return box;
    const auto x = box.x - (box.x % 2);
    const auto y = box.y - (box.y % 2);
    auto width = box.width + (box.x - x);
    auto height = box.height + (box.y - y);
    width += width % 2;
    height += height % 2;
    if (x + width > bounds.x + bounds.width) return std::nullopt;
    if (y + height > bounds.y + bounds.height) return std::nullopt;
    return Rect{x, y, width, height};
}

bool sameRow(const Fill& left, const Fill& right) {
    return left.at.y == right.at.y && left.at.height == right.at.height
        && left.at.x + left.at.width == right.at.x && left.rgba == right.rgba;
}

bool sameColumn(const Fill& above, const Fill& below) {
    return above.at.x == below.at.x && above.at.width == below.at.width
        && above.at.y + above.at.height == below.at.y && above.rgba == below.rgba;
}

/**
 * cuts that touch and share a colour become one fill: a grid whose top row is all
 * green thumbnails is one green rectangle, not eight. it is done in two passes
 * because that is enough for a grid, and a grid is what this exists for -- the
 * general case of merging rectangles is a different and much larger problem.
 */
std::vector<Fill> merge(std::vector<Fill> fills) {
    std::ranges::stable_sort(fills, [](const Fill& left, const Fill& right) {
        return left.at.y != right.at.y ? left.at.y < right.at.y : left.at.x < right.at.x;
    });

    std::vector<Fill> across;
    for (auto& fill : fills) {
        if (!across.empty() && sameRow(across.back(), fill)) {
            across.back().at.width += fill.at.width;
            across.back().solid = across.back().solid && fill.solid;
            continue;
        }
        across.push_back(fill);
    }

    std::ranges::stable_sort(across, [](const Fill& left, const Fill& right) {
        return left.at.x != right.at.x ? left.at.x < right.at.x : left.at.y < right.at.y;
    });

    std::vector<Fill> down;
    for (auto& fill : across) {
        if (!down.empty() && sameColumn(down.back(), fill)) {
            down.back().at.height += fill.at.height;
            down.back().solid = down.back().solid && fill.solid;
            continue;
        }
        down.push_back(fill);
    }
    return down;
}

/**
 * the shared shape of both colours: count, box, and keep the fill only when the
 * pixels it spares pay for the member it costs.
 */
template <typename Key>
Flat run(const std::vector<Rect>& cuts, const Rect& bounds, size_t bytesPerPixel, size_t worth,
         bool even, int tolerance, const Key invalid, auto&& keyAt, auto&& rgbaOf) {
    Flat flat;
    flat.boxes.reserve(cuts.size());
    std::vector<Fill> fills;

    for (const auto& cut : cuts) {
        const auto area = static_cast<size_t>(cut.width) * static_cast<size_t>(cut.height);
        const auto [background, count] = commonest<Key>(cut, invalid, keyAt);
        if (count == 0) {
            flat.boxes.push_back(cut);
            continue;
        }

        auto box = boxOf<Key>(cut, background, tolerance, keyAt);
        if (even) {
            const auto fitted = evenly(box, cut);
            if (!fitted) {
                flat.boxes.push_back(cut);
                continue;
            }
            box = *fitted;
        }

        const auto kept = static_cast<size_t>(box.width) * static_cast<size_t>(box.height);
        const auto spared = (area - kept) * bytesPerPixel;
        if (spared <= worth) {
            flat.boxes.push_back(cut);
            continue;
        }

        flat.boxes.push_back(box);
        fills.push_back(Fill{cut, rgbaOf(background), box.width == 0});
    }

    flat.fills = merge(std::move(fills));
    return flat;
}

}

namespace {

/**
 * the slack a row is given, from a figure held in sixteenths of a colour level.
 *
 * a whole level is too big a first step. letting every pixel within one level join
 * its run takes a fifth of the draws out of a photograph before anything finer has
 * been asked for, so a dial whose first click does that has a dead bottom third.
 *
 * the answer is to give the rows different slack and let the picture average them:
 * at five and a half, eight rows in sixteen get five and eight get six. which rows
 * is decided by stepping seven at a time around the sixteen, so the two kinds are
 * spread rather than banded -- a block of one followed by a block of the other is
 * the one arrangement that would show.
 *
 * it does not show in any case. the difference between a row at five and the same
 * row at six is a pixel here and there at the end of a run, which is below what the
 * neighbouring row already differs by.
 */
int slackOf(size_t row, int sixteenths) {
    return (sixteenths + static_cast<int>((row * 7) % 16)) / 16;
}

/**
 * one row of samples, with every pixel near enough to the run it is in replaced by
 * it.
 *
 * this is what buys draws, and it buys more of them than rounding to a grid does.
 * a grid moves every pixel in the picture and still only joins the ones that
 * happen to land on the same step; this moves only the pixels on the edge of a run
 * and joins them by construction. measured against each other at the same worst
 * error, the grid gave -26% of the draws and this gave -34%, with less average
 * error as well.
 *
 * it also has no contours. a grid draws its steps at the same values everywhere,
 * so a smooth gradient bands along fixed lines; a run follows the picture, so the
 * same gradient merely holds still a little longer before it moves on.
 */
void lengthen(uint8_t* row, size_t count, size_t stride, int tolerance) {
    if (tolerance <= 0 || count == 0) return;
    auto run = row[0];
    for (size_t at = 1; at < count; at++) {
        auto& sample = row[at * stride];
        const auto diff = static_cast<int>(sample) - static_cast<int>(run);
        if (diff <= tolerance && -diff <= tolerance) {
            sample = run;
        } else {
            run = sample;
        }
    }
}

}

void lengthen(Image<Color::RGBA8888>& img, int sixteenths) {
    if (sixteenths <= 0) return;
    const auto stride = static_cast<size_t>(img.width) * 4;
    for (int y = 0; y < img.height; y++) {
        const auto tolerance = slackOf(static_cast<size_t>(y), sixteenths);
        auto* row = img.data.data() + static_cast<size_t>(y) * stride;
        // the three channels move together: a pixel joins the run only when all of
        // them are near it, which is what the television compares
        auto run = row;
        for (int x = 1; x < img.width; x++) {
            auto* pixel = row + static_cast<size_t>(x) * 4;
            bool near = true;
            for (int channel = 0; channel < 3; channel++) {
                const auto diff = static_cast<int>(pixel[channel]) - static_cast<int>(run[channel]);
                if (diff > tolerance || -diff > tolerance) near = false;
            }
            if (near && pixel[3] == run[3]) {
                for (int channel = 0; channel < 3; channel++) pixel[channel] = run[channel];
            } else {
                run = pixel;
            }
        }
    }
}

void lengthen(Image<Color::YUV420>& img, int sixteenths) {
    if (sixteenths <= 0) return;
    const auto planes = Yuv420::planesOf(Size{img.width, img.height});
    auto* luma = img.data.data();
    auto* blue = luma + planes.luma;
    auto* red = blue + planes.chroma;

    for (size_t y = 0; y < planes.lumaHeight; y++) {
        lengthen(luma + y * planes.lumaWidth, planes.lumaWidth, 1, slackOf(y, sixteenths));
    }
    // the chroma planes are pictures of their own at half the size, and joining a
    // run in them joins the two by two blocks the television reads them from
    for (size_t y = 0; y < planes.chromaHeight; y++) {
        const auto tolerance = slackOf(y, sixteenths);
        lengthen(blue + y * planes.chromaWidth, planes.chromaWidth, 1, tolerance);
        lengthen(red + y * planes.chromaWidth, planes.chromaWidth, 1, tolerance);
    }
}

std::string spell(uint32_t rgba) {
    const auto part = [&](int shift) { return std::to_string((rgba >> shift) & 0xFFu); };
    return part(16) + " " + part(8) + " " + part(0) + " " + part(24);
}

Flat flatten(const Image<Color::RGBA8888>& img, const std::vector<Rect>& cuts, size_t worth, int tolerance) {
    const auto* data = img.data.data();
    const auto stride = static_cast<size_t>(img.width) * 4;

    const auto keyAt = [&](int x, int y) {
        const auto at = static_cast<size_t>(y) * stride + static_cast<size_t>(x) * 4;
        return (static_cast<uint32_t>(data[at + 3]) << 24) | (static_cast<uint32_t>(data[at]) << 16)
             | (static_cast<uint32_t>(data[at + 1]) << 8) | data[at + 2];
    };

    return run<uint32_t>(cuts, Rect{0, 0, img.width, img.height}, 4, worth, false, tolerance,
                         0xFFFFFFFFu, keyAt, [](uint32_t key) { return key; });
}

Flat flattenYuv(const Image<Color::YUV420>& img, const std::vector<Rect>& cuts, size_t worth, bool even, int tolerance) {
    const auto planes = Yuv420::planesOf(Size{img.width, img.height});
    const auto* luma = img.data.data();
    const auto* blue = luma + planes.luma;
    const auto* red = blue + planes.chroma;

    // two pixels are the same colour when their luma matches and they sit in a
    // block whose chroma matches, which is exact and costs no conversion
    const auto keyAt = [&](int x, int y) {
        const auto at = static_cast<size_t>(y) * planes.lumaWidth + static_cast<size_t>(x);
        const auto chroma = static_cast<size_t>(y / 2) * planes.chromaWidth + static_cast<size_t>(x / 2);
        return (static_cast<uint32_t>(luma[at]) << 16) | (static_cast<uint32_t>(blue[chroma]) << 8)
             | red[chroma];
    };

    /**
     * jfif colour, the same full range bt.601 the frames are written in -- and
     * rounded the same way the decoder rounds it, term by term.
     *
     * this is not pedantry. the fill travels as the three numbers a decoder paints
     * with, while every pixel around it travels as yuv for that decoder to convert
     * itself: if the two disagree by one, the ground is a shade off the picture
     * standing on it, and the seam shows at every edge.
     */
    const auto rgbaOf = [](uint32_t key) {
        const auto term = [](int weight, int value) {
            const auto scaled = weight * value;
            return (scaled < 0 ? scaled - 512 : scaled + 512) / 1024;
        };
        const auto clamp = [](int value) {
            return static_cast<uint32_t>(value < 0 ? 0 : (value > 255 ? 255 : value));
        };
        const auto y = static_cast<int>((key >> 16) & 0xFF);
        const auto u = static_cast<int>((key >> 8) & 0xFF) - 128;
        const auto v = static_cast<int>(key & 0xFF) - 128;
        const auto r = clamp(y + term(1436, v));
        const auto g = clamp(y - term(352, u) - term(731, v));
        const auto b = clamp(y + term(1815, u));
        return 0xFF000000u | (r << 16) | (g << 8) | b;
    };

    return run<uint32_t>(cuts, Rect{0, 0, img.width, img.height}, 2, worth, even, tolerance,
                         0xFFFFFFFFu, keyAt, rgbaOf);
}

}

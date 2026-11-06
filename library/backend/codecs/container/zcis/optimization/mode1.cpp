#include <haio/internal/codecs/zcis.hpp>

#include <algorithm>
#include <optional>
#include <string>

namespace Haio::Codecs::Zcis {

namespace {

/**
 * everything that is not the background, boxed.
 *
 * with slack the box is smaller and the pixels it leaves behind are painted the
 * background rather than themselves, which is the whole of what tolerance costs.
 */
Rect boxOf(const Rect& at, uint32_t background, int tolerance, auto&& keyAt) {
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
Flat run(const std::vector<Rect>& cuts, size_t bytesPerPixel, size_t worth, bool even, int tolerance,
         auto&& keyAt, auto&& rgbaOf) {
    Flat flat;
    flat.boxes.reserve(cuts.size());
    std::vector<Fill> fills;

    for (const auto& cut : cuts) {
        const auto area = static_cast<size_t>(cut.width) * static_cast<size_t>(cut.height);
        const auto [background, count] = commonest(cut, keyAt);
        if (count == 0) {
            flat.boxes.push_back(cut);
            continue;
        }

        auto box = boxOf(cut, background, tolerance, keyAt);
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
        fills.push_back(Fill{cut, rgbaOf(background), background, box.width == 0});
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
 */
int slackOf(size_t row, int sixteenths) {
    return (sixteenths + static_cast<int>((row * 7) % 16)) / 16;
}

/**
 * one row of a box, with every pixel pulled onto the nearest of the three things it
 * may be pulled onto.
 *
 * this is what buys draws, and it buys more of them than rounding to a grid does.
 * a grid moves every pixel in the picture and still only joins the ones that happen
 * to land on the same step; this moves only the pixels on the edge of a run and
 * joins them by construction. measured against each other at the same worst error,
 * the grid gave -26% of the draws and this gave -34%, with less average error too.
 *
 * it also has no contours. a grid draws its steps at the same values everywhere, so
 * a smooth gradient bands along fixed lines; a run follows the picture, so the same
 * gradient merely holds still a little longer before it moves on.
 *
 * what the grid did have was agreement between one row and the next, which is why it
 * came out in soft patches while a run on its own comes out in scratches. that is
 * what the third rule is for, and it costs nothing: a run that has already broken
 * can start anywhere, so it starts where the row above is.
 */
void walk(const Rect& box, int sixteenths, const std::vector<uint32_t>& ground, auto&& keyAt, auto&& put) {
    for (int y = box.y; y < box.y + box.height; y++) {
        const auto tolerance = slackOf(static_cast<size_t>(y - box.y), sixteenths);
        uint32_t run = 0;
        bool running = false;

        for (int x = box.x; x < box.x + box.width; x++) {
            const auto key = keyAt(x, y);
            const auto at = static_cast<size_t>(y - box.y) * static_cast<size_t>(box.width)
                          + static_cast<size_t>(x - box.x);

            if (!ground.empty() && !apart(key, ground[at], tolerance)) {
                put(x, y, ground[at]);
                run = ground[at];
                running = true;
                continue;
            }
            if (running && !apart(key, run, tolerance)) {
                put(x, y, run);
                continue;
            }
            // the row above, but only when taking it does not end the next run
            // before it starts: a value borrowed from above that the pixel after
            // cannot reach costs the draw that the whole arrangement exists to save
            if (y > box.y) {
                const auto above = keyAt(x, y - 1);
                const auto last = x + 1 == box.x + box.width;
                if (!apart(key, above, tolerance) && (last || !apart(keyAt(x + 1, y), above, tolerance))) {
                    put(x, y, above);
                    run = above;
                    running = true;
                    continue;
                }
            }
            run = key;
            running = true;
        }
    }
}

}

void lengthen(Image<Color::RGBA8888>& img, int sixteenths, const Flat& flat) {
    if (sixteenths <= 0) return;
    const auto stride = static_cast<size_t>(img.width) * 4;
    auto* data = img.data.data();
    const auto keyAt = keysOf(img);

    // the three channels move together: a pixel joins only when all of them are near,
    // which is what the television compares, and the alpha has to match outright
    const auto put = [&](int x, int y, uint32_t key) {
        auto* pixel = data + static_cast<size_t>(y) * stride + static_cast<size_t>(x) * 4;
        pixel[0] = static_cast<uint8_t>((key >> 16) & 0xFF);
        pixel[1] = static_cast<uint8_t>((key >> 8) & 0xFF);
        pixel[2] = static_cast<uint8_t>(key & 0xFF);
    };

    for (const auto& box : flat.boxes) {
        if (box.width <= 0 || box.height <= 0) continue;
        walk(box, sixteenths, groundOf(box, flat.fills), keyAt, put);
    }
}

void lengthen(Image<Color::YUV420>& img, int sixteenths, const Flat& flat) {
    if (sixteenths <= 0) return;
    const auto planes = Yuv420::planesOf(Size{img.width, img.height});
    auto* luma = img.data.data();
    auto* blue = luma + planes.luma;
    auto* red = blue + planes.chroma;
    const auto keyAt = keysOf(img);

    /**
     * the luma moves a pixel at a time and the chroma only by whole blocks.
     *
     * a two by two block shares one pair of chroma samples, so moving them for one
     * pixel moves them for its three neighbours as well. the luma therefore follows
     * the walk and the chroma only goes where the whole block agreed to go, which
     * costs a draw now and then and never a wrong pixel.
     */
    const auto put = [&](int x, int y, uint32_t key) {
        luma[static_cast<size_t>(y) * planes.lumaWidth + static_cast<size_t>(x)] =
            static_cast<uint8_t>((key >> 16) & 0xFF);
    };

    for (const auto& box : flat.boxes) {
        if (box.width <= 0 || box.height <= 0) continue;
        const auto ground = groundOf(box, flat.fills);
        std::vector<uint32_t> wanted(static_cast<size_t>(box.width) * static_cast<size_t>(box.height), 0);
        std::vector<uint8_t> moved(wanted.size(), 0);

        walk(box, sixteenths, ground, keyAt, [&](int x, int y, uint32_t key) {
            const auto at = static_cast<size_t>(y - box.y) * static_cast<size_t>(box.width)
                          + static_cast<size_t>(x - box.x);
            wanted[at] = key;
            moved[at] = 1;
        });

        for (int y = box.y; y < box.y + box.height; y += 2) {
            for (int x = box.x; x < box.x + box.width; x += 2) {
                const auto wide = std::min(2, box.x + box.width - x);
                const auto tall = std::min(2, box.y + box.height - y);
                const auto first = static_cast<size_t>(y - box.y) * static_cast<size_t>(box.width)
                                 + static_cast<size_t>(x - box.x);
                bool all = moved[first] != 0;
                for (int dy = 0; dy < tall && all; dy++) {
                    for (int dx = 0; dx < wide && all; dx++) {
                        const auto at = static_cast<size_t>(y + dy - box.y) * static_cast<size_t>(box.width)
                                      + static_cast<size_t>(x + dx - box.x);
                        all = moved[at] != 0 && wanted[at] == wanted[first];
                    }
                }
                for (int dy = 0; dy < tall; dy++) {
                    for (int dx = 0; dx < wide; dx++) {
                        const auto at = static_cast<size_t>(y + dy - box.y) * static_cast<size_t>(box.width)
                                      + static_cast<size_t>(x + dx - box.x);
                        if (moved[at]) put(x + dx, y + dy, wanted[at]);
                    }
                }
                if (!all) continue;
                const auto chroma = static_cast<size_t>(y / 2) * planes.chromaWidth + static_cast<size_t>(x / 2);
                blue[chroma] = static_cast<uint8_t>((wanted[first] >> 8) & 0xFF);
                red[chroma] = static_cast<uint8_t>(wanted[first] & 0xFF);
            }
        }
    }
}

std::string spell(uint32_t rgba) {
    const auto part = [&](int shift) { return std::to_string((rgba >> shift) & 0xFFu); };
    return part(16) + " " + part(8) + " " + part(0) + " " + part(24);
}

Flat flatten(const Image<Color::RGBA8888>& img, const std::vector<Rect>& cuts, size_t worth, int tolerance) {
    return run(cuts, 4, worth, false, tolerance, keysOf(img), [](uint32_t key) { return key; });
}

Flat flattenYuv(const Image<Color::YUV420>& img, const std::vector<Rect>& cuts, size_t worth, bool even, int tolerance) {
    return run(cuts, 2, worth, even, tolerance, keysOf(img), rgbaOfYuv);
}

}

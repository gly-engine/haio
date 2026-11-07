#include <haio/internal/codecs/zcis.hpp>

#include <algorithm>
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
 * row of every rectangle that travels, less the ones the mask spares.
 *
 * the width of a run does not appear because it does not cost anything -- a
 * television draws one pixel and a hundred for the same price, which is the whole
 * reason any of this exists. a spared run is subtracted rather than looked up pixel
 * by pixel because it is worth exactly one call wherever it sits: at the end of a
 * row it ends a call, and in the middle of one it splits a call in two.
 */
size_t drawsOf(const std::vector<Rect>& boxes, const Flat* flat, bool stretched, auto&& keyAt) {
    size_t count = 0;
    for (const auto& box : boxes) {
        if (box.width <= 0 || box.height <= 0) continue;
        for (int y = box.y; y < box.y + box.height; y++) {
            const auto paints = paintsOf(box, y, stretched);
            auto run = keyAt(box.x, y);
            count += paints;
            for (int x = box.x + 1; x < box.x + box.width; x++) {
                const auto key = keyAt(x, y);
                if (key == run) continue;
                run = key;
                count += paints;
            }
        }
    }
    if (!flat) return count;

    count += flat->fills.size();
    for (const auto& mask : flat->masks) count -= std::min(count, mask.spared);
    return count;
}

/**
 * the least colour that buys what was asked for.
 *
 * nought is tried on its own first because it is the answer whenever the ground and
 * the mask are enough, and neither of those costs any colour at all: a grid of
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

/**
 * the share of the draws the number asks to be rid of, counted against the picture
 * as it would travel with nothing done to it at all.
 */
size_t targetOf(size_t plain, int wanted) {
    return plain - plain * static_cast<size_t>(wanted) / 100;
}

/**
 * the level the number is measured at, which is not always the level being written.
 *
 * the number says how much colour to give up, and the ground of a cut costs none: it
 * names a colour that was already there and sends less of it, so it belongs inside
 * what the number promises. the mask is a different kind of saving. it costs no
 * colour either, but it costs bytes, and it is asked for separately -- so letting it
 * count towards the number would mean that turning it on made the file no faster,
 * only gentler, which is not what someone who turned it on was after.
 *
 * the same goes for the preview, which is asked for separately too and costs neither
 * colour nor bytes. so the search plans at one, in the layout the picture travels in
 * by default, and whatever anything else finds is added on top rather than spent.
 */
int measuredAt(int level) {
    return level < 1 ? level : 1;
}

}

Plan<Color::RGBA8888> planOf(const Image<Color::RGBA8888>& img, const std::vector<Rect>& cuts, size_t worth,
                             int level, int sixteenths, bool stretched) {
    Plan<Color::RGBA8888> plan{img, {}};
    const auto tolerance = sixteenths / 16;

    if (level >= 1) plan.flat = flatten(plan.img, cuts, worth, tolerance);
    else plan.flat.boxes = cuts;

    // and only then is the colour given up: extending the runs first would leave
    // every row of the ground a shade of its own and nothing worth naming
    lengthen(plan.img, sixteenths, plan.flat);
    if (level >= 2) maskAll(plan.img, plan.flat, worth, stretched);
    return plan;
}

Plan<Color::YUV420> planOfYuv(const Image<Color::YUV420>& img, const std::vector<Rect>& cuts, size_t worth,
                              bool even, int level, int sixteenths) {
    Plan<Color::YUV420> plan{img, {}};
    const auto tolerance = sixteenths / 16;

    if (level >= 1) plan.flat = flattenYuv(plan.img, cuts, worth, even, tolerance);
    else plan.flat.boxes = cuts;

    lengthen(plan.img, sixteenths, plan.flat);
    if (level >= 2) maskAllYuv(plan.img, plan.flat, worth);
    return plan;
}

int minify(const Image<Color::RGBA8888>& img, const std::vector<Rect>& cuts, size_t worth, int level, int wanted) {
    if (wanted <= 0 || cuts.empty()) return 0;

    const auto target = targetOf(drawsOf(cuts, nullptr, true, keysOf(img)), wanted);
    return least(target, [&](int sixteenths) {
        // always against the layout the picture travels in by default, so that the
        // number means the same thing whatever else is asked for
        const auto plan = planOf(img, cuts, worth, measuredAt(level), sixteenths, true);
        return drawsOf(plan.flat.boxes, &plan.flat, true, keysOf(plan.img));
    });
}

int minifyYuv(const Image<Color::YUV420>& img, const std::vector<Rect>& cuts, size_t worth, bool even, int level,
              int wanted) {
    if (wanted <= 0 || cuts.empty()) return 0;

    const auto target = targetOf(drawsOf(cuts, nullptr, false, keysOf(img)), wanted);
    return least(target, [&](int sixteenths) {
        const auto plan = planOfYuv(img, cuts, worth, even, measuredAt(level), sixteenths);
        return drawsOf(plan.flat.boxes, &plan.flat, false, keysOf(plan.img));
    });
}

}

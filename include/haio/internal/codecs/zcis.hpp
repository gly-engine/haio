#pragma once

#include <haio/internal/codecs/yuv.hpp>
#include <haio_codec.hpp>
#include <haio_object.hpp>

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <vector>

/**
 * what the zcis encoder works out before it sends a pixel, when it is asked to.
 *
 * the whole idea is that a picture of a thing on a plain background is mostly the
 * background, and the background costs the same as the thing: a televison draws a
 * run of one colour and a run of a hundred for the same price, so ten thousand
 * pixels of white are ten thousand prices. naming the colour once and sending only
 * the part that varies is the difference between a page that loads and a page that
 * freezes.
 */
namespace Haio::Codecs::Zcis {

/** a rectangle that is one colour, and the colour it is */
struct Fill {
    Rect at;
    uint32_t rgba = 0;  /**< 0xAARRGGBB, as the member name spells it out in hex */
    uint32_t key = 0;   /**< the same colour as the picture holds it, which is what pixels are compared to */

    /** nothing paints over this one, so it may be the commit its row was waiting for */
    bool solid = false;
};

/**
 * the pixels of a box that the ground already shows, so that nothing need be drawn
 * over them.
 *
 * it travels as a one bit pcx, which is the one shape of mask the standard has and
 * the one the televisions read.
 */
struct Mask {
    Rect at;
    std::vector<uint8_t> pcx;  /**< the bitmap, already written, since whether it was worth sending needed its size */
    size_t spared = 0;         /**< how many draws it takes out, which is one for every run it covers */

    /**
     * whether it goes down after the base layer rather than before it.
     *
     * the base layer is stretched over both rows of every pair, so by the time the
     * sharp layer arrives the even rows have already been painted -- with the colour
     * of their odd neighbour. wherever the two rows of a pair ended up the same, that
     * paint was already right and the sharp layer is redrawing it for nothing. a mask
     * laid between the two says so, and it cannot touch the base layer because that
     * one has already been read.
     */
    bool refine = false;
};

/**
 * the flat ground and what is left of each cut.
 *
 * boxes are in the order the cuts came in, one each, and a box the size of its cut
 * means that cut found no ground worth naming.
 */
struct Flat {
    std::vector<Fill> fills;
    std::vector<Rect> boxes;
    std::vector<Mask> masks;
};

/**
 * the three bytes of a key, far enough apart to matter.
 *
 * the fourth is compared exactly rather than loosely, because it is the alpha and
 * two pixels of the same colour at different transparencies are not the same pixel:
 * covering one with the other turns a hole into paint.
 */
inline bool apart(uint32_t left, uint32_t right, int tolerance) {
    if ((left >> 24) != (right >> 24)) return true;
    for (int shift = 0; shift <= 16; shift += 8) {
        const auto a = static_cast<int>((left >> shift) & 0xFF);
        const auto b = static_cast<int>((right >> shift) & 0xFF);
        if (a - b > tolerance || b - a > tolerance) return true;
    }
    return false;
}

/** the colour of a pixel as the television compares them, for a whole picture */
inline auto keysOf(const Image<Color::RGBA8888>& img) {
    return [data = img.data.data(), stride = static_cast<size_t>(img.width) * 4](int x, int y) {
        const auto at = static_cast<size_t>(y) * stride + static_cast<size_t>(x) * 4;
        return (static_cast<uint32_t>(data[at + 3]) << 24) | (static_cast<uint32_t>(data[at]) << 16)
             | (static_cast<uint32_t>(data[at + 1]) << 8) | data[at + 2];
    };
}

/**
 * the same, where the chroma is shared by every two by two block.
 *
 * two pixels are the same colour when their luma matches and they sit in a block
 * whose chroma matches, which is exact and costs no conversion.
 */
inline auto keysOf(const Image<Color::YUV420>& img) {
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
 * jfif colour, the same full range bt.601 the frames are written in -- and rounded
 * the same way the decoder rounds it, term by term.
 *
 * this is not pedantry. the fill travels as the three numbers a decoder paints with,
 * while every pixel around it travels as yuv for that decoder to convert itself: if
 * the two disagree by one, the ground is a shade off the picture standing on it, and
 * the seam shows at every edge.
 */
inline uint32_t rgbaOfYuv(uint32_t key) {
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
}

/**
 * the most common colour in a rectangle, and how much of it there is.
 *
 * the whole rectangle is counted rather than its border because a border is a guess:
 * a thumbnail with a shadow or a rounded corner has a border that is not the
 * background, and a picture of the sky has a background that is nowhere near its
 * border. counting is cheap next to what is saved.
 */
inline std::pair<uint32_t, size_t> commonest(const Rect& at, auto&& keyAt) {
    std::unordered_map<uint32_t, size_t> seen;
    for (int y = at.y; y < at.y + at.height; y++) {
        for (int x = at.x; x < at.x + at.width; x++) seen[keyAt(x, y)]++;
    }

    uint32_t best = 0;
    size_t most = 0;
    for (const auto& [key, count] : seen) {
        if (count > most) {
            most = count;
            best = key;
        }
    }
    return {best, most};
}

/** the fill a box stands on, which is the last one written that covers all of it */
inline const Fill* under(const std::vector<Fill>& fills, const Rect& box) {
    const Fill* found = nullptr;
    for (const auto& fill : fills) {
        if (fill.at.x <= box.x && fill.at.y <= box.y
            && fill.at.x + fill.at.width >= box.x + box.width
            && fill.at.y + fill.at.height >= box.y + box.height) {
            found = &fill;
        }
    }
    return found;
}

/**
 * the colour under every pixel of a box, painted the way the decoder paints it: the
 * fills in the order they were written, each one over the last.
 *
 * it is a plane rather than a search per pixel because the question is asked once for
 * every pixel of every box, twice over, and a plane answers it by reading a number.
 */
inline std::vector<uint32_t> groundOf(const Rect& box, const std::vector<Fill>& fills) {
    const auto* start = under(fills, box);
    if (!start) return {};

    std::vector<uint32_t> ground(static_cast<size_t>(box.width) * static_cast<size_t>(box.height), start->key);
    for (const auto& fill : fills) {
        const auto left = std::max(box.x, fill.at.x);
        const auto top = std::max(box.y, fill.at.y);
        const auto right = std::min(box.x + box.width, fill.at.x + fill.at.width);
        const auto bottom = std::min(box.y + box.height, fill.at.y + fill.at.height);
        for (int y = top; y < bottom; y++) {
            const auto row = static_cast<size_t>(y - box.y) * static_cast<size_t>(box.width);
            std::fill_n(ground.begin() + static_cast<std::ptrdiff_t>(row + static_cast<size_t>(left - box.x)),
                        right - left, fill.key);
        }
    }
    return ground;
}

/**
 * what a draw is worth in bytes, on the television this was written for.
 *
 * the two levels pull opposite ways -- the first spends draws to save bytes, the
 * second spends bytes to save draws -- and without a rate between them there is no
 * saying whether either is worth doing. so it is written down, derived rather than
 * chosen, from two things that were measured on the set.
 *
 * the draw is solid: ninety six microseconds for a colour and a rectangle, whatever
 * the rectangle's width. the byte is the arguable one. the sheet arrives over the
 * wire in about three quarters of a microsecond a byte, and that is what is used
 * here; the same sheet takes forty one seconds to reach the lua that asked for it,
 * which is thirty microseconds a byte and would say the mask is never worth sending.
 * the difference is not the network but the reader, which concatenates its buffer
 * and so costs the square of what it fetches -- a fault being fixed, and not one the
 * shape of a file should be bent around.
 *
 * if the reader is the one that matters on some other set, this is the one number to
 * change, and every weighing in here follows it.
 */
constexpr size_t drawMicroseconds = 96;
constexpr size_t byteCentimicroseconds = 77;
constexpr size_t bytesPerDraw = drawMicroseconds * 100 / byteCentimicroseconds;

/**
 * the picture as rgba, cut by cut.
 *
 * @param worth how many bytes of pixels a fill has to save to be worth its own
 *              member, which is sixty of ar header plus the colour and the padding
 */
Flat flatten(const Image<Color::RGBA8888>& img, const std::vector<Rect>& cuts, size_t worth, int tolerance);

/**
 * the same, for a picture whose chroma is shared by every two by two block.
 *
 * @param even whether the boxes have to start and end on even pixels, which a 4:2:0
 *             frame needs and a cut of it therefore needs too
 */
Flat flattenYuv(const Image<Color::YUV420>& img, const std::vector<Rect>& cuts, size_t worth, bool even, int tolerance);

/**
 * which pixels the ground already shows, found once the runs have been extended and
 * nothing moves any more.
 *
 * a pixel is spared when it is exactly its ground, never merely near it: by this
 * point everything that was near has been pulled onto it, so an exact comparison is
 * the whole of what is there and costs no second thought about what the viewer sees.
 */
void maskAll(const Image<Color::RGBA8888>& img, Flat& flat, size_t worth, bool stretched);
void maskAllYuv(const Image<Color::YUV420>& img, Flat& flat, size_t worth);

/**
 * how many times the decoder paints one row of a box.
 *
 * the progressive split is not free and this is where it shows. the base layer holds
 * the odd rows and is stretched over the whole box, so the decoder paints each of
 * its rows into two rows of the target; the layer that follows holds the even rows
 * and paints each of them once. so a box of rgb costs half as much drawing again as
 * the picture has rows, and a single 4:2:0 frame costs exactly its rows.
 *
 * it matters for every weighing in here. a run spared on an odd row is two calls to
 * the television and one on an even row is one, and a mask that is worth sending for
 * the one may not be for the other.
 *
 * a picture asked for a half preview sends both layers interlaced instead, one on
 * the even rows and one on the odd, and then nothing is painted twice and every row
 * is worth one.
 */
inline size_t paintsOf(const Rect& box, int y, bool stretched) {
    if (!stretched || box.height <= 1) return 1;
    return (y - box.y) % 2 == 1 ? 2 : 1;
}

/** the colour as the fill member is named: RRGGBBAA in upper case hex */
std::string spell(uint32_t rgba);

/**
 * every pixel near enough to the run it sits in replaced by it, row by row.
 *
 * it happens before anything is measured so that the ground, the boxes and the
 * pixels all agree about what colour a pixel is.
 *
 * a pixel has three things it may be pulled onto, and they are tried in this order:
 * the ground under it, the run it sits in, and the pixel above it. the ground comes
 * first so that the background between two things on a shelf stays the one colour it
 * was and does not come out scratched -- and so that the mask can spare it. the run
 * comes next because joining it is the only one of the three that removes a draw. the
 * pixel above is the one that costs nothing either way, and it is what turns the
 * leftovers from stripes into patches: where a run has broken anyway, starting the
 * next one on the colour the row above settled at makes the two rows agree, and the
 * eye reads a soft blotch rather than a scratch.
 *
 * @param sixteenths how far a pixel may be from any of the three, in sixteenths of a
 *                   colour level, since a whole level is too coarse a first step: the
 *                   rows take the two whole numbers it falls between, spread so that
 *                   the picture averages to the fraction asked for
 * @param flat where the boxes are and what ground is under them; only what travels is
 *             touched, since nothing else is ever drawn
 */
void lengthen(Image<Color::RGBA8888>& img, int sixteenths, const Flat& flat);
void lengthen(Image<Color::YUV420>& img, int sixteenths, const Flat& flat);

/** a picture as it will travel, and everything that goes with it */
template <Color P>
struct Plan {
    Image<P> img;
    Flat flat;
};

/**
 * everything the encoder does to a picture at one level and one slack, in one place
 * so that what the number is measured against and what is written out cannot drift
 * apart.
 */
Plan<Color::RGBA8888> planOf(const Image<Color::RGBA8888>& img, const std::vector<Rect>& cuts, size_t worth,
                             int level, int sixteenths, bool stretched);
Plan<Color::YUV420> planOfYuv(const Image<Color::YUV420>& img, const std::vector<Rect>& cuts, size_t worth,
                              bool even, int level, int sixteenths);

/**
 * the colour to give up so that the picture costs the asked for share less to draw.
 *
 * the number names the saving, not the means: zero is the picture to the bit, and
 * every other value is a percentage of the draws the plain picture would cost. the
 * encoder spends what costs nothing first -- the flat ground and the mask, which
 * remove draws without moving a pixel -- and only reaches for colour when that falls
 * short, so the same number damages less as the encoder learns to save more.
 *
 * @param level which commands the file may carry, since a fill or a mask it may not
 *              write is a saving it cannot count towards the number
 * @param wanted the share of draws to remove, nought to ninety nine
 * @return the slack to give up, in sixteenths of a colour level: nought when none
 *         is needed and the ceiling when the picture cannot reach what was asked
 */
int minify(const Image<Color::RGBA8888>& img, const std::vector<Rect>& cuts, size_t worth, int level, int wanted);
int minifyYuv(const Image<Color::YUV420>& img, const std::vector<Rect>& cuts, size_t worth, bool even, int level,
              int wanted);

}

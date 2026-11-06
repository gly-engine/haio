#pragma once

#include <haio_codec.hpp>
#include <haio_object.hpp>

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
    uint32_t rgba = 0;  /**< 0xAARRGGBB, as the .txt spells it out in decimal */

    /** nothing paints over this one, so it may be the commit its row was waiting for */
    bool solid = false;
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
};

/**
 * the picture as rgba, cut by cut.
 *
 * @param worth how many bytes of pixels a fill has to save to be worth its own
 *              member, which is sixty of ar header plus the colour and the padding
 */
Flat flatten(const Image<Color::RGBA8888>& img, const std::vector<Rect>& cuts, size_t worth, int tolerance);

/**
 * the same, for a picture whose chroma is shared by every two by two block. the
 * comparison happens in yuv so that nothing is converted a pixel at a time, and
 * only the colour that wins becomes rgb.
 *
 * @param even whether the boxes have to start and end on even pixels, which a 4:2:0
 *             frame needs and a cut of it therefore needs too
 */
Flat flattenYuv(const Image<Color::YUV420>& img, const std::vector<Rect>& cuts, size_t worth, bool even, int tolerance);

/** the colour as the .txt holds it: four decimal numbers, red first */
std::string spell(uint32_t rgba);

/**
 * the colour to give up so that the picture costs the asked for share less to draw.
 *
 * the number names the saving, not the means: zero is the picture to the bit, and
 * every other value is a percentage of the draws the plain picture would cost. the
 * encoder spends what costs nothing first -- the flat ground, which removes draws
 * without moving a pixel -- and only reaches for colour when that falls short, so
 * the same number damages less as the encoder learns to save more.
 *
 * @param level which commands the file may carry, since a fill it may not write is
 *              a saving it cannot count towards the number
 * @param wanted the share of draws to remove, nought to ninety nine
 * @return the slack to give up, in sixteenths of a colour level: nought when none
 *         is needed and the ceiling when the picture cannot reach what was asked
 */
int minify(const Image<Color::RGBA8888>& img, const std::vector<Rect>& cuts, size_t worth, int level, int wanted);
int minifyYuv(const Image<Color::YUV420>& img, const std::vector<Rect>& cuts, size_t worth, bool even, int level, int wanted);

/**
 * every pixel near enough to the run it sits in replaced by it, row by row.
 *
 * it happens before anything is measured so that the ground, the boxes and the
 * pixels all agree about what colour a pixel is.
 *
 * @param sixteenths how far a pixel may be from its run, in sixteenths of a colour
 *                   level, since a whole level is too coarse a first step: the rows
 *                   take the two whole numbers it falls between, spread so that the
 *                   picture averages to the fraction asked for
 */
void lengthen(Image<Color::RGBA8888>& img, int sixteenths);
void lengthen(Image<Color::YUV420>& img, int sixteenths);

}

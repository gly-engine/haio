#pragma once

#include "haio_codec.hpp"

#include <span>
#include <vector>

/**
 * @defgroup atlas Atlas
 * laying several pictures out on one canvas, so a container that can say where each
 * one is may carry them all.
 *
 * the layout is geometry and nothing else: it is handed sizes and gives back
 * rectangles, and knows nothing about the container that will write them.
 */
namespace Haio::Atlas {

/**
 * where every picture ended up, in the order they were given, and how big the canvas
 * has to be to hold them.
 */
struct Layout {
    Size canvas;
    std::vector<Rect> rects;
};

/**
 * every picture starts on a multiple of this, so a cut never lands mid-word. it is
 * small on purpose: what a picture wastes is the few pixels up to the next multiple,
 * never a whole cell of whatever the smallest one happened to be.
 */
inline constexpr int alignment = 4;

/**
 * shelf packing: the tallest first, filling a row until it runs out of width and
 * starting another underneath. the width is not guessed but tried, every shelf a
 * picture's width could open, and the one that wastes least is kept.
 *
 * it is not the tightest packing there is, and the signature is what a tighter one
 * would have: a skyline reads the same sizes and gives back the same rectangles.
 */
Result<Layout> shelf(std::span<const Size> sizes);

}

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
 * the room every picture was given, in the order they were given, and how big the
 * canvas has to be to hold them. a shelf gives each exactly its own size; a grid gives
 * each a whole cell, and where in it the picture sits is the caller's to say.
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

/** no canvas is laid out bigger than this on a side, whatever was asked for */
inline constexpr int largest = 16384;

/**
 * how far apart the pictures are: the room each takes is rounded up to a multiple
 * of quantum, and gap is left between one and the next.
 */
struct Spread {
    Size quantum{1, 1};
    Size gap;
};

/**
 * shelf packing: the tallest first, filling a row until it runs out of width and
 * starting another underneath. the width is not guessed but tried, every shelf a
 * picture's width could open, and the one that wastes least is kept.
 *
 * it is not the tightest packing there is, and the signature is what a tighter one
 * would have: a skyline reads the same sizes and gives back the same rectangles.
 */
Result<Layout> shelf(std::span<const Size> sizes, Spread spread);

/** the same, every picture starting on a multiple of alignment and nothing between them */
Result<Layout> shelf(std::span<const Size> sizes);

/**
 * a fixed grid, filled a row at a time in the order the pictures were given, so the
 * one at n is always in the same cell whatever its neighbours are. every cell is as
 * big as the largest picture, rounded up to the quantum.
 *
 * cells is columns by rows, and zero is as many as it takes. a fixed number of
 * columns keeps the canvas that wide even when fewer pictures came, because a tileset
 * is read by its width; the rows shrink to the ones that were filled, and a picture
 * past the last of them is an error rather than a row nobody asked for.
 */
Result<Layout> grid(std::span<const Size> sizes, Size cells, Spread spread);

}

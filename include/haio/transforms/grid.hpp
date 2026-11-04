#pragma once

#include "haio/stage.hpp"
#include "haio/transforms/composite.hpp"

namespace Haio::Stages {

inline constexpr Option gridOptions[] = {
    gravityOption,
};

/**
 * every picture of the parenthesis laid on one canvas, and where each of them went
 * kept as its cuts, so a container with an index can cut them back out and one
 * without simply shows the sheet.
 *
 * the value is `[columns]@[rows]`, then `-W-H` to round every cell up to a multiple,
 * then `+X+Y` to leave a gap between cells: `@` lets the layout pick, packing them
 * tight and in any order; `8@` is eight columns, `@1` one row, `1@` one column, and
 * any of those keeps the order they were given in. `@1-32-32+2+2` is a strip of 32
 * pixel cells with two between them.
 *
 * @haio{convert} assets/jucagato256x256.png -resize 64x64 assets/disk256x256color.png -resize 32x32 -gravity south -grid @1+4+0 png:-
 */
inline constexpr Stage grid{
    .spellings = {"grid"},
    .rule = "grid",
    .takes = "grid-geometry",
    .options = gridOptions,
    .help = "lay every picture on one canvas: @ packs them, 8@ is eight columns, @1 one row; -W-H rounds a cell, +X+Y spaces them",
    .merges = {1, static_cast<size_t>(-1)},
};

template <> Built build<&grid>(std::string_view value, const Given& given);

}

#pragma once

#include "haio_transform.hpp"
#include "haio/stage.hpp"

#include <algorithm>
#include <utility>

namespace Haio {

/**
 * which edge or corner a layer is measured from, named the way imagemagick's
 * -gravity names them; the names on the command line are these, lowered.
 */
enum class Gravity {
    NorthWest,
    North,
    NorthEast,
    West,
    Center,
    East,
    SouthWest,
    South,
    SouthEast,

    /** imagemagick's two ways of saying there is no gravity, which measure from the top left */
    None,
    Forget,
};

/**
 * where a layer's top left corner lands, the way imagemagick works it out: the
 * offset is measured away from the edge the gravity names, so +10 under an east
 * gravity moves the layer left, and a centred axis adds it to the middle.
 */
constexpr std::pair<int, int> placeOf(Size base, Size layer, Gravity gravity, int dx, int dy) {
    const auto column = [&](int left, int middle, int right) {
        switch (gravity) {
            case Gravity::NorthWest: case Gravity::West: case Gravity::SouthWest:
            case Gravity::None: case Gravity::Forget: return left;
            case Gravity::North: case Gravity::Center: case Gravity::South: return middle;
            case Gravity::NorthEast: case Gravity::East: case Gravity::SouthEast: return right;
        }
        return left;
    };
    const auto row = [&](int top, int middle, int bottom) {
        switch (gravity) {
            case Gravity::NorthWest: case Gravity::North: case Gravity::NorthEast:
            case Gravity::None: case Gravity::Forget: return top;
            case Gravity::West: case Gravity::Center: case Gravity::East: return middle;
            case Gravity::SouthWest: case Gravity::South: case Gravity::SouthEast: return bottom;
        }
        return top;
    };
    return {column(dx, (base.width - layer.width) / 2 + dx, base.width - layer.width - dx),
            row(dy, (base.height - layer.height) / 2 + dy, base.height - layer.height - dy)};
}

/**
 * one picture over another, the way imagemagick's default -compose over does it:
 * straight alpha, the layer's coverage first and whatever shows through after. the
 * part of the layer that falls outside the base is dropped, as it is there.
 */
inline Image<Color::RGBA8888> composeOver(Image<Color::RGBA8888> base, const Image<Color::RGBA8888>& layer,
                                          int x, int y) {
    const int x0 = std::max(0, x);
    const int y0 = std::max(0, y);
    const int x1 = std::min(base.width, x + layer.width);
    const int y1 = std::min(base.height, y + layer.height);

    for (int by = y0; by < y1; by++) {
        for (int bx = x0; bx < x1; bx++) {
            auto* under = base.data.data() + (static_cast<size_t>(by) * static_cast<size_t>(base.width) + static_cast<size_t>(bx)) * 4;
            const auto* over = layer.data.data()
                             + (static_cast<size_t>(by - y) * static_cast<size_t>(layer.width) + static_cast<size_t>(bx - x)) * 4;

            const int la = over[3];
            if (la == 255) {
                std::copy_n(over, 4, under);
                continue;
            }
            if (la == 0) continue;

            // everything scaled by 255 so the sums stay whole numbers
            const int ba = under[3] * (255 - la) / 255;
            const int alpha = la + ba;
            for (int c = 0; c < 3; c++) {
                under[c] = static_cast<uint8_t>((over[c] * la + under[c] * ba + alpha / 2) / alpha);
            }
            under[3] = static_cast<uint8_t>(alpha);
        }
    }
    return base;
}

namespace Transforms {

/**
 * one per colour the base can be, in `library/backend/transforms/composite/COLOUR.cpp`.
 * the layer is always rgba8888: it has come out of its own pipeline, and it is the
 * layer's alpha that says where it covers.
 */
template <Color P>
Result<Image<P>> Composite(Image<P> base, const Image<Color::RGBA8888>& layer, Gravity gravity, int x, int y) = delete;

template <Color P>
concept Composable = requires (Image<P> b, const Image<Color::RGBA8888>& l, Gravity g, int x) {
    { Composite<P>(std::move(b), l, g, x, x) } -> std::same_as<Result<Image<P>>>;
};

}

namespace Stages {

inline constexpr Option compositeOptions[] = {
    {.spellings = {"geometry"}, .takes = "offset", .help = "how far the layer is moved from where gravity puts it, as +X+Y",
     .fallback = "+0+0"},
    {.spellings = {"gravity"}, .takes = "name", .help = "which edge or corner the offset is measured from",
     .shape = Shape::Name, .names = namesOf<Gravity>(), .fallback = "northwest", .called = "gravity type"},
};

/** the picture from the parenthesis before it, laid over the one before that */
inline constexpr Stage composite{
    .spellings = {"composite"},
    .rule = "composite",
    .options = compositeOptions,
    .help = "lay the picture in the parenthesis before it over this one",
    .merges = true,
};

template <> Built build<&composite>(std::string_view value, const Given& given);

}

}

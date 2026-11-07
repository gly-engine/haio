#pragma once

#include "haio_transform.hpp"
#include "haio/stage.hpp"
#include "haio/transforms/resize/nearest.hpp"

#include <optional>

namespace Haio {

/**
 * how a resize picks its pixels, named the way imagemagick names its -filter.
 *
 * the names people type are the enumerators, lowered, so a new filter is an
 * enumerator here, a kernel beside resize/nearest.hpp and a case in each colour's
 * source -- and nothing in the parser or the grammar.
 */
enum class ResizeFilter {
    Point,   /**< nearest neighbour: no mixing, so it runs on anything addressable */
};

/**
 * what a size of both sides means, the way imagemagick reads its geometry flags.
 *
 *   WxH   fit: the largest that fits inside, keeping the shape
 *   WxH!  exact: both sides as written, the shape given up
 *   WxH^  cover: the smallest that covers both, keeping the shape
 *   WxHp  pad: fit inside, then centred on WxH exactly, the sides filled with the
 *         background the edge of the picture shows, or left transparent when it shows none
 */
enum class ResizeFit {
    Inside,
    Exact,
    Cover,
    Pad,
};

/** the filter somebody typed, in any case, or nothing when no filter has that name */
constexpr std::optional<ResizeFilter> resizeFilterNamed(std::string_view name) {
    return Stages::enumNamed<ResizeFilter>(name);
}

namespace Transforms {

/**
 * one per colour it runs on, in `library/backend/transforms/resize/COLOUR.cpp`.
 *
 * the size is already a size here: a share such as 30% is worked out by the pipeline,
 * which is the first place that knows what it is a share of.
 */
template <Color P> Result<Image<P>> Resize(Image<P> image, Size size, ResizeFilter filter) = delete;

template <Color P>
concept Resizable = requires (Image<P> i, Size s, ResizeFilter f) {
    { Resize<P>(std::move(i), s, f) } -> std::same_as<Result<Image<P>>>;
};

}

namespace Stages {

inline constexpr Option resizeOptions[] = {
    {.spellings = {"filter"}, .takes = "name", .help = "how the pixels are picked, as imagemagick names it",
     .shape = Shape::Name, .names = namesOf<ResizeFilter>(), .fallback = "point", .called = "image filter"},
};

inline constexpr Stage resize{
    .spellings = {"resize"},
    .rule = "resize",
    .takes = "resize-geometry",
    .options = resizeOptions,
    .help = "resize, to fit WxH, to WxH! exactly, to cover WxH^, to fit and pad to WxHp, to 100x or x100 keeping its shape, or to a share such as 30pct",
};

template <> Built build<&resize>(std::string_view value, const Given& given);

}

}

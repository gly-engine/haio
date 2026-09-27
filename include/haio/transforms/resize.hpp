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
    .takes = "size",
    .options = resizeOptions,
    .help = "resize, to WxH or to a share such as 30% or 30pct",
};

template <> Built build<&resize>(std::string_view value, const Given& given);

}

}

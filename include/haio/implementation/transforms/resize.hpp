#pragma once

#include "haio/implementation/wip/dither.hpp"
#include "haio/implementation/transforms.hpp"
#include "haio/util/family.hpp"
#include "haio/implementation/transforms/resize/nearest.hpp"
#include "haio/util/option.hpp"

#include <optional>

namespace Haio::Implementation {

/** the filter somebody typed, in any case, or nothing when no filter has that name */
constexpr std::optional<Transforms::Transform<>::ResizeFilter> resizeFilterNamed(std::string_view name) {
    return Util::enumNamed<Transforms::Transform<>::ResizeFilter>(name);
}

}

namespace Haio::Implementation::Transforms {

/**
 * a picture at another size: Transform<Operation::Resize, Color::RGBA8888>{}(image, size, filter), one class per
 * colour it runs on, in `library/implementation/transforms/resize/COLOUR.cpp`.
 *
 * the size is already a size here: a share such as 30% is worked out by the pipeline,
 * which is the first place that knows what it is a share of.
 */
template <Color P>
concept Resizable = Util::MemberOf<Transform<Operation::Resize, P>, Transform<>>
    && requires (const Transform<Operation::Resize, P>& resize, Image<P> i, Size s, Transform<>::ResizeFilter f) {
        { resize(std::move(i), s, f) } -> std::same_as<Result<Image<P>>>;
    };

}

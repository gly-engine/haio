#pragma once

#include "haio/implementation/wip/dither.hpp"
#include "haio/implementation/transforms.hpp"
#include "haio/implementation/language/stage.hpp"

namespace Haio::Implementation::LanguageStage {

inline constexpr Option compositeOptions[] = {
    {.spellings = {"geometry"}, .takes = "offset", .help = "how far the layer is moved from where gravity puts it, as +X+Y",
     .fallback = "+0+0"},
    {.spellings = {"gravity"}, .takes = "name", .help = "which edge or corner the offset is measured from",
     .shape = OptionShape::Name, .names = Util::namesOf<Transforms::Transform<>::Gravity>(), .fallback = "northwest", .called = "gravity type"},
    {.spellings = {"compose"}, .takes = "name",
     .help = "how the layer and the picture under it are mixed",
     .shape = OptionShape::Name, .names = Util::namesOf<Transforms::Compose>(), .fallback = "over", .called = "compose operator"},
};

/** "dst src -composite" lays src over dst, and "dst src1 src2" src1 tinted by src2 */
inline constexpr Stage composite{
    .spellings = {"composite"},
    .rule = "composite",
    .options = compositeOptions,
    .help = "lay the picture after the first over it, or the second tinted by the third",
    .merges = true,
};

template <> Built build<&composite>(std::string_view value, const Given& given);

}

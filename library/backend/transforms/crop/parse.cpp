#include <haio/transforms/crop.hpp>
#include <haio_pipeline.hpp>
#include <haio_string.hpp>

namespace Haio::Stages {

/** both ways of writing a rectangle, since both are already out there */
template <>
Built build<&crop>(std::string_view value, const Given&) {
    Rect rect;
    if (!String::tryGetCropGeometry(value, rect) && !String::tryGetRect(value, rect)) {
        return std::unexpected(Refusal{"invalid argument for option `-crop': " + std::string(value), std::string(value)});
    }
    return Tokens::Crop(rect);
}

}

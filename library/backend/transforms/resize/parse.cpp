#include <haio/transforms/resize.hpp>
#include <haio_pipeline.hpp>
#include <haio_string.hpp>

namespace Haio::Stages {

template <>
Built build<&resize>(std::string_view value, const Given& given) {
    auto filter = ResizeFilter::Point;
    if (const auto* named = given.find("filter")) {
        const auto found = resizeFilterNamed(named->value);
        if (!found) return std::unexpected(Refusal{"unrecognized image filter " + quoted(named->value), named->value});
        filter = *found;
    }

    // a share first, since "30%" is not a size and never parses as one
    if (const auto share = String::getPercent(value); share != 0) return Tokens::ResizeByPercent(share, filter);

    Size size;
    if (!String::tryGetSize(value, size)) {
        return std::unexpected(Refusal{"invalid argument for option `-resize': " + std::string(value), std::string(value)});
    }
    return Tokens::Resize(size, filter);
}

}

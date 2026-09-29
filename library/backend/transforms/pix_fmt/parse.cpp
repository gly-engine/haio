#include <haio/transforms/pix_fmt.hpp>
#include <haio_pipeline.hpp>
#include <haio_registry.hpp>

namespace Haio::Stages {

template <>
Built build<&pixFmt>(std::string_view value, const Given&) {
    const auto color = colorNamed(value);
    if (!color) return std::unexpected(Refusal{"unrecognized pixel format " + quoted(value), std::string(value)});
    return Tokens::PixFmt(*color);
}

}

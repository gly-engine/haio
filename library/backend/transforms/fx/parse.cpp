#include <haio/transforms/fx.hpp>
#include <haio_pipeline.hpp>

namespace Haio::Stages {

template <>
Built build<&fx>(std::string_view value, const Given&) {
    return Tokens::Fx(std::string(value));
}

}

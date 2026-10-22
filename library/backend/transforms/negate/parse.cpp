#include <haio/transforms/negate.hpp>
#include <haio_pipeline.hpp>

namespace Haio::Stages {

template <>
Built build<&negate>(std::string_view, const Given&) {
    return Tokens::Negate();
}

}

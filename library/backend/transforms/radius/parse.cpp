#include <haio/transforms/radius.hpp>
#include <haio_pipeline.hpp>
#include <haio_string.hpp>

#include <stdexcept>

namespace Haio::Stages {

template <>
Built build<&radius>(std::string_view value, const Given&) {
    try {
        return Tokens::Radius(String::getInt(value));
    } catch (const std::exception&) {
        return std::unexpected(Refusal{"invalid argument for option `-radius': " + std::string(value), std::string(value)});
    }
}

}

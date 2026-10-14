#include <haio/implementation/language/stages/radius.hpp>
#include <haio/implementation/transforms/radius.hpp>
#include <haio/implementation/language/token.hpp>
#include <haio/util/string.hpp>

#include <stdexcept>

namespace Haio::Implementation::LanguageStage {

template <>
Built build<&radius>(std::string_view value, const Given&) {
    try {
        return LanguageToken::Radius(Util::getInt(value));
    } catch (const std::exception&) {
        return std::unexpected(Refusal{"invalid argument for option `-radius': " + std::string(value), std::string(value)});
    }
}

}

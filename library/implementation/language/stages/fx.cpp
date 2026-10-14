#include <haio/implementation/language/stages/fx.hpp>
#include <haio/implementation/language/token.hpp>

namespace Haio::Implementation::LanguageStage {

template <>
Built build<&fx>(std::string_view value, const Given&) {
    return LanguageToken::Fx(std::string(value));
}

}

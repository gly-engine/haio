#include <haio/implementation/language/stages/negate.hpp>
#include <haio/implementation/transforms/negate.hpp>
#include <haio/implementation/language/token.hpp>

namespace Haio::Implementation::LanguageStage {

template <>
Built build<&negate>(std::string_view, const Given&) {
    return LanguageToken::Negate();
}

}

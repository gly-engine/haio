#include <haio/implementation/language/stages/resize.hpp>
#include <haio/implementation/transforms/resize.hpp>
#include <haio/implementation/language/token.hpp>
#include <haio/util/string.hpp>

namespace Haio::Implementation::LanguageStage {

template <>
Built build<&resize>(std::string_view value, const Given& given) {
    auto filter = Transforms::Transform<>::ResizeFilter::Point;
    if (const auto* named = given.find("filter")) {
        const auto found = resizeFilterNamed(named->value);
        if (!found) return std::unexpected(Refusal{"unrecognized image filter " + Util::quoted(named->value), named->value});
        filter = *found;
    }

    // a share first, since "30%" is not a size and never parses as one
    if (const auto share = Util::getPercent(value); share != 0) return LanguageToken::ResizeByPercent(share, filter);

    Size size;
    if (!Util::tryGetSize(value, size)) {
        return std::unexpected(Refusal{"invalid argument for option `-resize': " + std::string(value), std::string(value)});
    }
    return LanguageToken::Resize(size, filter);
}

}

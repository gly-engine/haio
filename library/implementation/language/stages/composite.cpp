#include <haio/implementation/language/stages/composite.hpp>
#include <haio/implementation/transforms/composite.hpp>
#include <haio/implementation/language/token.hpp>

#include <charconv>
#include <optional>

namespace Haio::Implementation::LanguageStage {
namespace {

/** "+10", "-3": a sign, always written, and a whole number after it */
std::optional<int> signedPart(std::string_view text, size_t& at) {
    if (at >= text.size() || (text[at] != '+' && text[at] != '-')) return std::nullopt;
    const bool negative = text[at] == '-';
    const auto* first = text.data() + at + 1;
    int value = 0;
    const auto [end, problem] = std::from_chars(first, text.data() + text.size(), value);
    if (problem != std::errc{} || end == first) return std::nullopt;
    at = static_cast<size_t>(end - text.data());
    return negative ? -value : value;
}

}

/**
 * -geometry is an offset here, +X+Y as imagemagick writes it. its size form, which
 * scales the layer first, is not taken: -resize inside the parenthesis says the
 * same thing where it can be seen.
 */
template <>
Built build<&composite>(std::string_view, const Given& given) {
    int x = 0;
    int y = 0;
    if (const auto* geometry = given.find("geometry")) {
        size_t at = 0;
        const std::string_view text = geometry->value;
        const auto dx = signedPart(text, at);
        const auto dy = dx ? signedPart(text, at) : std::nullopt;
        if (!dx || !dy || at != text.size()) {
            return std::unexpected(Refusal{"invalid argument for option `-geometry': " + geometry->value, geometry->value});
        }
        x = *dx;
        y = *dy;
    }

    auto gravity = Transforms::Transform<>::Gravity::NorthWest;
    if (const auto* named = given.find("gravity")) gravity = *Util::enumNamed<Transforms::Transform<>::Gravity>(named->value);

    auto compose = Transforms::Compose::Over;
    if (const auto* named = given.find("compose")) compose = *Util::enumNamed<Transforms::Compose>(named->value);

    return LanguageToken::Composite(compose, gravity, x, y);
}

}

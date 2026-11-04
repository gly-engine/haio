#include <haio/transforms/grid.hpp>
#include <haio_pipeline.hpp>

#include <charconv>
#include <optional>

namespace Haio::Stages {
namespace {

/** a run of digits at the front, or zero when there is none */
std::optional<int> count(std::string_view& text) {
    if (text.empty() || text.front() < '0' || text.front() > '9') return 0;
    int value = 0;
    const auto [end, problem] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (problem != std::errc{} || value <= 0) return std::nullopt;
    text.remove_prefix(static_cast<size_t>(end - text.data()));
    return value;
}

/**
 * two numbers each behind the sign given, as "-32-32" or "+2+2". a space is a plus,
 * the way it is in every geometry, since a url writes one as the other.
 */
std::optional<Size> pair(std::string_view& text, char sign) {
    const auto signed_ = [&](char c) { return c == sign || (sign == '+' && c == ' '); };
    if (text.empty() || !signed_(text.front())) return Size{};

    Size out;
    for (auto* side : {&out.width, &out.height}) {
        if (text.empty() || !signed_(text.front())) return std::nullopt;
        text.remove_prefix(1);
        const auto [end, problem] = std::from_chars(text.data(), text.data() + text.size(), *side);
        if (problem != std::errc{} || end == text.data()) return std::nullopt;
        text.remove_prefix(static_cast<size_t>(end - text.data()));
    }
    return out;
}

}

template <>
Built build<&grid>(std::string_view value, const Given& given) {
    const auto refused = [&] {
        return std::unexpected(Refusal{"invalid argument for option `-grid': " + std::string(value), std::string(value)});
    };

    auto text = value;
    const auto columns = count(text);
    if (!columns || text.empty() || text.front() != '@') return refused();
    text.remove_prefix(1);
    const auto rows = count(text);
    const auto quantum = rows ? pair(text, '-') : std::nullopt;
    const auto gap = quantum ? pair(text, '+') : std::nullopt;
    if (!gap || !text.empty()) return refused();
    if ((quantum->width == 0) != (quantum->height == 0)) return refused();

    auto gravity = Gravity::NorthWest;
    if (const auto* named = given.find("gravity")) gravity = *enumNamed<Gravity>(named->value);

    const Size multiple = quantum->width == 0 ? Size{1, 1} : *quantum;
    return Tokens::Grid(Size{*columns, *rows}, multiple, *gap, gravity);
}

}

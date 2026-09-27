#include <haio/transforms/palette.hpp>
#include <haio_palette.hpp>
#include <haio_pipeline.hpp>
#include <haio_string.hpp>

#include <stdexcept>

namespace Haio::Stages {
namespace {

/** a count, which has to be a whole positive number and nothing else */
std::optional<size_t> countOf(std::string_view value) {
    try {
        const auto number = String::getInt(value);
        if (number <= 0) return std::nullopt;
        return static_cast<size_t>(number);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

}

/**
 * -filter is required, -limit is not.
 *
 * the strategy of a limit is written out, the same way the filter is. "sort:16" and
 * "spread:16" give visibly different pictures of the same photograph: one spends the
 * budget where there is the most area, the other where there is the most that is new.
 * there is no answer that is right often enough to be assumed, and a bare "16" would
 * be haio choosing what somebody's picture looks like.
 */
template <>
Built build<&palette>(std::string_view value, const Given& given) {
    const auto* filter = given.find("filter");
    const auto how = ditherNamed(filter->value);
    // the parser checked the name against the declaration already
    if (!how) return std::unexpected(Refusal{"unrecognized dither method " + quoted(filter->value), filter->value});

    size_t most = 0;
    auto limitHow = Limit::Spread;
    if (const auto* limit = given.find("limit")) {
        const std::string_view spelled = limit->value;
        const auto colon = spelled.find(':');
        if (colon == std::string_view::npos) {
            return std::unexpected(Refusal{"invalid argument for option `-limit': " + limit->value, limit->value});
        }
        const auto named = limitNamed(spelled.substr(0, colon));
        if (!named) return std::unexpected(Refusal{"invalid argument for option `-limit': " + limit->value, limit->value});

        const auto count = countOf(spelled.substr(colon + 1));
        if (!count) return std::unexpected(Refusal{"invalid argument for option `-limit': " + limit->value, limit->value});

        most = *count;
        limitHow = *named;
    }

    return Tokens::Palette(std::string(value), *how, most, limitHow);
}

}

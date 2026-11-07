#include <haio/transforms/blur.hpp>
#include <haio_pipeline.hpp>

#include <charconv>
#include <cmath>
#include <optional>

namespace Haio::Stages {
namespace {

std::optional<double> number(std::string_view text) {
    double value = 0;
    const auto* end = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(text.data(), end, value);
    if (text.empty() || ec != std::errc{} || ptr != end || value < 0 || !std::isfinite(value)) return std::nullopt;
    return value;
}

/**
 * three box passes of width w spread a pixel as far as a gaussian of variance
 * (w * w - 1) / 4, so the width that matches a sigma is the root of 4 sigma squared
 * plus one, and the radius is half of what it has beyond the centre.
 */
int boxRadiusOf(double sigma) {
    return static_cast<int>(std::lround((std::sqrt(4 * sigma * sigma + 1) - 1) / 2));
}

}

template <>
Built build<&blur>(std::string_view value, const Given&) {
    const auto refuse = [&] {
        return std::unexpected(Refusal{"invalid argument for option `-blur': " + std::string(value), std::string(value)});
    };

    const auto sep = value.find_first_of("xX");
    const auto radius = number(value.substr(0, sep));
    if (sep == std::string_view::npos) {
        if (!radius) return refuse();
        return Tokens::Blur(boxRadiusOf(1.0));
    }

    const auto sigma = number(value.substr(sep + 1));
    if (!sigma || (sep != 0 && !radius)) return refuse();
    return Tokens::Blur(boxRadiusOf(*sigma));
}

}

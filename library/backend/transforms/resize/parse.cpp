#include <haio/transforms/resize.hpp>
#include <haio_pipeline.hpp>
#include <haio_string.hpp>

#include <charconv>
#include <optional>

namespace Haio::Stages {
namespace {

struct Geometry {
    Size size;
    ResizeFit fit = ResizeFit::Inside;
};

/**
 * a side is a number of pixels, or nothing at all, or a negative number: the last two
 * both mean "work it out from the other side and the picture's shape", and the
 * negative says what to round the answer up to, so -2 keeps it even.
 */
bool side(std::string_view text, int& out) {
    if (text.empty()) {
        out = 0;
        return true;
    }

    int value = 0;
    const auto* begin = text.data();
    const auto* end = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc{} || ptr != end || value == 0) return false;

    out = value;
    return true;
}

std::optional<Geometry> geometry(std::string_view value) {
    Geometry out;
    if (value.ends_with('!')) {
        out.fit = ResizeFit::Exact;
        value.remove_suffix(1);
    } else if (value.ends_with('^')) {
        out.fit = ResizeFit::Cover;
        value.remove_suffix(1);
    } else if (value.ends_with('p')) {
        out.fit = ResizeFit::Pad;
        value.remove_suffix(1);
    }

    const auto sep = value.find_first_of("xX:");
    if (sep == std::string_view::npos) return std::nullopt;

    if (!side(value.substr(0, sep), out.size.width)) return std::nullopt;
    if (!side(value.substr(sep + 1), out.size.height)) return std::nullopt;

    if (out.size.width <= 0 && out.size.height <= 0) return std::nullopt;
    const bool bothSides = out.fit == ResizeFit::Exact || out.fit == ResizeFit::Pad;
    if (bothSides && (out.size.width <= 0 || out.size.height <= 0)) return std::nullopt;
    return out;
}

}

template <>
Built build<&resize>(std::string_view value, const Given& given) {
    auto filter = ResizeFilter::Point;
    if (const auto* named = given.find("filter")) {
        const auto found = resizeFilterNamed(named->value);
        if (!found) return std::unexpected(Refusal{"unrecognized image filter " + quoted(named->value), named->value});
        filter = *found;
    }

    // a share first, since "30%" is not a size and never parses as one
    if (const auto share = String::getPercent(value); share != 0) return Tokens::ResizeByPercent(share, filter);

    const auto asked = geometry(value);
    if (!asked) {
        return std::unexpected(Refusal{"invalid argument for option `-resize': " + std::string(value), std::string(value)});
    }
    return Tokens::Resize(asked->size, filter, asked->fit);
}

}

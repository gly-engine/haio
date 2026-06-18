#include <haio_string.hpp>

#include <charconv>
#include <stdexcept>
#include <vector>

namespace {

std::vector<std::string_view> split(std::string_view value, char sep) {
    std::vector<std::string_view> parts;
    while (true) {
        const auto pos = value.find(sep);
        parts.push_back(value.substr(0, pos));
        if (pos == std::string_view::npos) break;
        value.remove_prefix(pos + 1);
    }
    return parts;
}

bool parseInt(std::string_view value, int& out) {
    if (value.empty()) return false;
    if (value.front() == '+') value.remove_prefix(1);

    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto [ptr, ec] = std::from_chars(begin, end, out);
    return ec == std::errc{} && ptr == end;
}

}

namespace Haio::String {

int getInt(std::string_view value) {
    int out = 0;
    if (!parseInt(value, out)) throw std::runtime_error("invalid integer: " + std::string(value));
    return out;
}

bool tryGetSize(std::string_view value, Size& out) {
    const auto sep = value.find_first_of("xX");
    if (sep == std::string_view::npos) {
        int side = 0;
        if (!parseInt(value, side) || side <= 0) return false;
        out = {side, side};
        return true;
    }

    int width = 0;
    int height = 0;
    if (!parseInt(value.substr(0, sep), width) || !parseInt(value.substr(sep + 1), height)) return false;
    if (width <= 0 || height <= 0) return false;
    out = {width, height};
    return true;
}

Size getSize(std::string_view value) {
    Size out;
    if (!tryGetSize(value, out)) throw std::runtime_error("invalid size: " + std::string(value));
    return out;
}

bool tryGetRect(std::string_view value, Rect& out) {
    const auto parts = split(value, ',');
    if (parts.size() != 4) return false;

    Rect rect;
    if (!parseInt(parts[0], rect.x)) return false;
    if (!parseInt(parts[1], rect.y)) return false;
    if (!parseInt(parts[2], rect.width)) return false;
    if (!parseInt(parts[3], rect.height)) return false;
    if (rect.width <= 0 || rect.height <= 0) return false;

    out = rect;
    return true;
}

Rect getRect(std::string_view value) {
    Rect out;
    if (!tryGetRect(value, out)) throw std::runtime_error("invalid rect: " + std::string(value));
    return out;
}

bool tryGetCropGeometry(std::string_view value, Rect& out) {
    const auto sep = value.find_first_of("xX");
    if (sep == std::string_view::npos) return false;

    int width = 0;
    int height = 0;
    if (!parseInt(value.substr(0, sep), width)) return false;
    if (width <= 0) return false;

    auto rest = value.substr(sep + 1);
    const auto offset = rest.find_first_of("+-");
    if (!parseInt(rest.substr(0, offset), height)) return false;
    if (height <= 0) return false;

    Rect rect{.width = width, .height = height};
    if (offset == std::string_view::npos) {
        out = rect;
        return true;
    }

    rest.remove_prefix(offset);
    const auto second = rest.substr(1).find_first_of("+-");
    if (second == std::string_view::npos) return false;

    const auto x = rest.substr(0, second + 1);
    const auto y = rest.substr(second + 1);
    if (!parseInt(x, rect.x) || !parseInt(y, rect.y)) return false;

    out = rect;
    return true;
}

}

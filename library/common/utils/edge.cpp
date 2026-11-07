#include <haio_util.hpp>

#include <algorithm>
#include <cstdlib>

namespace Haio::Util {

std::optional<std::array<uint8_t, 4>> edgeColour(const Image<Color::RGBA8888>& image) {
    constexpr int tolerance = 8;
    if (image.width <= 0 || image.height <= 0) return std::nullopt;

    std::array<std::vector<uint8_t>, 4> channels;
    const auto take = [&](int x, int y) {
        const auto* px = image.data.data() + (static_cast<size_t>(y) * image.width + x) * 4;
        for (size_t c = 0; c < 4; c++) channels[c].push_back(px[c]);
    };
    for (int x = 0; x < image.width; x++) {
        take(x, 0);
        if (image.height > 1) take(x, image.height - 1);
    }
    for (int y = 1; y + 1 < image.height; y++) {
        take(0, y);
        if (image.width > 1) take(image.width - 1, y);
    }

    const auto count = channels[0].size();
    std::array<uint8_t, 4> median{};
    for (size_t c = 0; c < 4; c++) {
        auto sorted = channels[c];
        std::ranges::nth_element(sorted, sorted.begin() + static_cast<std::ptrdiff_t>(count / 2));
        median[c] = sorted[count / 2];
    }

    size_t near = 0;
    for (size_t at = 0; at < count; at++) {
        bool close = true;
        for (size_t c = 0; c < 4; c++) close = close && std::abs(channels[c][at] - median[c]) <= tolerance;
        if (close) near++;
    }
    if (near * 10 < count * 9) return std::nullopt;
    return median;
}

}

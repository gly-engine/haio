#include <haio.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

void requireRgba(const Haio::Image& image, std::string_view op) {
    if (image.type != Haio::Format::RGBA8888) {
        throw std::runtime_error(std::string(op) + " expects rgba8888 image");
    }
    const auto expected = static_cast<size_t>(image.width) * static_cast<size_t>(image.height) * 4;
    if (image.width <= 0 || image.height <= 0 || image.data.size() != expected) {
        throw std::runtime_error(std::string(op) + " got invalid image data");
    }
}

}

namespace Haio {

Image cropImage(const Image& image, Rect rect) {
    requireRgba(image, "crop");
    const int x0 = std::clamp(rect.x, 0, image.width);
    const int y0 = std::clamp(rect.y, 0, image.height);
    const int x1 = std::clamp(rect.x + rect.width, x0, image.width);
    const int y1 = std::clamp(rect.y + rect.height, y0, image.height);
    const int width = x1 - x0;
    const int height = y1 - y0;

    std::vector<uint8_t> out(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    for (int y = 0; y < height; y++) {
        const auto src = (static_cast<size_t>(y0 + y) * static_cast<size_t>(image.width) + static_cast<size_t>(x0)) * 4;
        const auto dst = static_cast<size_t>(y) * static_cast<size_t>(width) * 4;
        std::copy_n(image.data.data() + src, static_cast<size_t>(width) * 4, out.data() + dst);
    }
    return Image{Format::RGBA8888, width, height, std::move(out)};
}

Image resizeImage(const Image& image, Size size) {
    requireRgba(image, "resize");
    if (size.width <= 0 && size.height <= 0) throw std::runtime_error("resize expects a positive size");

    int width = size.width;
    int height = size.height;
    if (width <= 0) width = std::max(1, image.width * height / image.height);
    if (height <= 0) height = std::max(1, image.height * width / image.width);

    std::vector<uint8_t> out(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    for (int y = 0; y < height; y++) {
        const int sy = std::min(image.height - 1, y * image.height / height);
        for (int x = 0; x < width; x++) {
            const int sx = std::min(image.width - 1, x * image.width / width);
            const auto src = (static_cast<size_t>(sy) * static_cast<size_t>(image.width) + static_cast<size_t>(sx)) * 4;
            const auto dst = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
            std::copy_n(image.data.data() + src, 4, out.data() + dst);
        }
    }
    return Image{Format::RGBA8888, width, height, std::move(out)};
}

Image roundImageCorners(const Image& image, int radius) {
    requireRgba(image, "radius");
    if (radius <= 0) return image;

    Image out = image;
    const int r = std::min(radius, std::min(image.width, image.height) / 2);
    const int r2 = r * r;

    auto maskCorner = [&](int x, int y, int cx, int cy) {
        const int dx = x - cx;
        const int dy = y - cy;
        if (dx * dx + dy * dy > r2) {
            const auto off = (static_cast<size_t>(y) * static_cast<size_t>(out.width) + static_cast<size_t>(x)) * 4 + 3;
            out.data[off] = 0;
        }
    };

    for (int y = 0; y < r; y++) {
        for (int x = 0; x < r; x++) {
            maskCorner(x, y, r - 1, r - 1);
            maskCorner(out.width - 1 - x, y, out.width - r, r - 1);
            maskCorner(x, out.height - 1 - y, r - 1, out.height - r);
            maskCorner(out.width - 1 - x, out.height - 1 - y, out.width - r, out.height - r);
        }
    }

    return out;
}

}

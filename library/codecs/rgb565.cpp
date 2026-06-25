#include <haio.hpp>

#include <stdexcept>

namespace {

/** @todo remove in future, validation should move to the compile-time graph when it exists */
static void validateRGBA(const Haio::Image& img) {
    if (img.type != Haio::Format::RGBA8888) {
        throw std::runtime_error("rgb565 encode only accepts rgba8888 images");
    }
    if (img.width <= 0 || img.height <= 0 || img.data.size() != static_cast<size_t>(img.width) * static_cast<size_t>(img.height) * 4) {
        throw std::runtime_error("invalid rgba8888 image for rgb565 encode");
    }
}

/** @todo remove in future, validation should move to the compile-time graph when it exists */
static void validateRGB565(const Haio::Image& img) {
    if (img.type != Haio::Format::RGB565) {
        throw std::runtime_error("rgb565 decode only accepts rgb565 images");
    }
    if (img.width <= 0 || img.height <= 0 || img.data.size() != static_cast<size_t>(img.width) * static_cast<size_t>(img.height) * 2) {
        throw std::runtime_error("invalid rgb565 image");
    }
}

}

namespace Haio {

template <>
Stage Encode<Format::RGB565>() {
    return [](const Image& img) {
        validateRGBA(img);
        std::vector<uint8_t> buffer;
        Buffer::Copy<Format::RGBA8888, Format::RGB565>(img.data, buffer);
        return Image{Format::RGB565, img.width, img.height, std::move(buffer)};
    };
}

template <>
Stage Decode<Format::RGB565>() {
    return [](const Image& img) {
        validateRGB565(img);
        std::vector<uint8_t> buffer;
        Buffer::Copy<Format::RGB565, Format::RGBA8888>(img.data, buffer);
        return Image{Format::RGBA8888, img.width, img.height, std::move(buffer)};
    };
}

}

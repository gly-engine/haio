#include <haio.hpp>

#include <cstdint>
#include <stdexcept>

namespace Haio {

namespace {

uint8_t expand5(uint16_t x) {
    return static_cast<uint8_t>((x * 255 + 15) / 31);
}

uint8_t expand6(uint16_t x) {
    return static_cast<uint8_t>((x * 255 + 31) / 63);
}

void validateRGBA(const Image& img) {
    if (img.type != Format::RGBA8888) {
        throw std::runtime_error("rgb565 encode only accepts rgba8888 images");
    }
    if (img.width <= 0 || img.height <= 0 || img.data.size() != static_cast<size_t>(img.width) * static_cast<size_t>(img.height) * 4) {
        throw std::runtime_error("invalid rgba8888 image for rgb565 encode");
    }
}

}

template <>
Stage Encode<Format::RGB565>() {
    return [](const Image& img) {
        validateRGBA(img);

        std::vector<uint8_t> buffer(static_cast<size_t>(img.width) * static_cast<size_t>(img.height) * 2);
        for (size_t src = 0, dst = 0; src < img.data.size(); src += 4, dst += 2) {
            const auto r = static_cast<uint16_t>((img.data[src + 0] * 31 + 127) / 255);
            const auto g = static_cast<uint16_t>((img.data[src + 1] * 63 + 127) / 255);
            const auto b = static_cast<uint16_t>((img.data[src + 2] * 31 + 127) / 255);
            const auto px = static_cast<uint16_t>((r << 11) | (g << 5) | b);
            buffer[dst + 0] = static_cast<uint8_t>(px);
            buffer[dst + 1] = static_cast<uint8_t>(px >> 8);
        }

        return Image{Format::RGB565, img.width, img.height, std::move(buffer)};
    };
}

template <>
Stage Decode<Format::RGB565>() {
    return [](const Image& img) {
        if (img.type != Format::RGB565) {
            throw std::runtime_error("rgb565 decode only accepts rgb565 images");
        }
        if (img.width <= 0 || img.height <= 0 || img.data.size() != static_cast<size_t>(img.width) * static_cast<size_t>(img.height) * 2) {
            throw std::runtime_error("invalid rgb565 image");
        }

        std::vector<uint8_t> buffer(static_cast<size_t>(img.width) * static_cast<size_t>(img.height) * 4);
        for (size_t src = 0, dst = 0; src < img.data.size(); src += 2, dst += 4) {
            const auto px = static_cast<uint16_t>(img.data[src + 0] | (img.data[src + 1] << 8));
            buffer[dst + 0] = expand5((px >> 11) & 0x1f);
            buffer[dst + 1] = expand6((px >> 5) & 0x3f);
            buffer[dst + 2] = expand5(px & 0x1f);
            buffer[dst + 3] = 255;
        }

        return Image{Format::RGBA8888, img.width, img.height, std::move(buffer)};
    };
}

}

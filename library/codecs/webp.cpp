#include <haio.hpp>

#include <webp/decode.h>
#include <webp/encode.h>

#include <stdexcept>

namespace Haio {

template <>
Stage Decode<Format::WEBP>() {
    return [](const Image& img) {
        int width = 0;
        int height = 0;
        uint8_t* decoded = WebPDecodeRGBA(img.data.data(), img.data.size(), &width, &height);
        if (!decoded || width <= 0 || height <= 0) {
            throw std::runtime_error("failed to decode webp");
        }

        std::vector<uint8_t> buffer(decoded, decoded + static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
        WebPFree(decoded);
        return Image{Format::RGBA8888, width, height, std::move(buffer)};
    };
}

template <>
Stage Encode<Format::WEBP>() {
    return [](const Image& img) {
        if (img.type != Format::RGBA8888) {
            throw std::runtime_error("webp encode only accepts rgba8888 images");
        }
        if (img.width <= 0 || img.height <= 0 || img.data.size() != static_cast<size_t>(img.width) * static_cast<size_t>(img.height) * 4) {
            throw std::runtime_error("invalid rgba8888 image for webp encode");
        }

        uint8_t* out = nullptr;
        const auto len = WebPEncodeLosslessRGBA(img.data.data(), img.width, img.height, img.width * 4, &out);
        if (!out || len == 0) {
            throw std::runtime_error("failed to encode webp");
        }

        std::vector<uint8_t> buffer(out, out + len);
        WebPFree(out);
        return Image{Format::WEBP, img.width, img.height, std::move(buffer)};
    };
}

}

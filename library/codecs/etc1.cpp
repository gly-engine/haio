#include <haio.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

#include <Decode.hpp>
#include <ProcessRGB.hpp>

static int paddedSize(int size) {
    return (size + 3) & ~3;
}

static size_t blockCount(int width, int height) {
    return static_cast<size_t>(paddedSize(width) / 4) * static_cast<size_t>(paddedSize(height) / 4);
}

static void validateDimensions(int width, int height) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("etc1 images require positive width and height");
    }
}

static std::vector<uint32_t> makePaddedBGRA(const Haio::Image& img, int paddedWidth, int paddedHeight) {
    const auto expectedSize = static_cast<size_t>(img.width) * static_cast<size_t>(img.height) * 4;
    if (img.data.size() != expectedSize) {
        throw std::runtime_error("invalid rgba8888 data size for etc1 encode");
    }

    std::vector<uint32_t> pixels(static_cast<size_t>(paddedWidth) * static_cast<size_t>(paddedHeight));
    for (int y = 0; y < paddedHeight; y++) {
        const int srcY = std::min(y, img.height - 1);
        for (int x = 0; x < paddedWidth; x++) {
            const int srcX = std::min(x, img.width - 1);
            const auto srcOffset = (static_cast<size_t>(srcY) * static_cast<size_t>(img.width) + static_cast<size_t>(srcX)) * 4;
            const uint8_t bgra[4] = {
                img.data[srcOffset + 2],
                img.data[srcOffset + 1],
                img.data[srcOffset + 0],
                img.data[srcOffset + 3]
            };
            std::memcpy(&pixels[static_cast<size_t>(y) * static_cast<size_t>(paddedWidth) + static_cast<size_t>(x)], bgra, 4);
        }
    }
    return pixels;
}


namespace Haio {

template <>
Stage Encode<Format::ETC1>() {
    return [](const Image& img) {
        if (img.type != Format::RGBA8888) {
            throw std::runtime_error("etc1 encode only accepts rgba8888 images");
        }
        validateDimensions(img.width, img.height);

        const int paddedWidth = paddedSize(img.width);
        const int paddedHeight = paddedSize(img.height);
        const size_t blocks = blockCount(img.width, img.height);
        if (blocks > std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error("etc1 image is too large");
        }

        auto pixels = makePaddedBGRA(img, paddedWidth, paddedHeight);
        std::vector<uint64_t> compressed(blocks);

        CompressEtc1RgbDither(
            pixels.data(),
            compressed.data(),
            static_cast<uint32_t>(blocks),
            static_cast<size_t>(paddedWidth)
        );

        std::vector<uint8_t> buffer(blocks * 8);
        std::memcpy(buffer.data(), compressed.data(), buffer.size());

        return Image{Format::ETC1, img.width, img.height, std::move(buffer)};
    };
}

template <>
Stage Decode<Format::ETC1>() {
    return [](const Image& img) {
        if (img.type != Format::ETC1) {
            throw std::runtime_error("etc1 decode only accepts etc1 images");
        }
        validateDimensions(img.width, img.height);

        const size_t blocks = blockCount(img.width, img.height);
        const size_t expectedSize = blocks * 8;
        if (img.data.size() != expectedSize) {
            throw std::runtime_error("invalid etc1 data size");
        }

        const int paddedWidth = paddedSize(img.width);
        const int paddedHeight = paddedSize(img.height);
        std::vector<uint64_t> compressed(blocks);
        std::memcpy(compressed.data(), img.data.data(), expectedSize);

        std::vector<uint32_t> paddedRGBA(static_cast<size_t>(paddedWidth) * static_cast<size_t>(paddedHeight));
        DecodeRGB(compressed.data(), paddedRGBA.data(), paddedWidth, paddedHeight);

        std::vector<uint8_t> buffer(static_cast<size_t>(img.width) * static_cast<size_t>(img.height) * 4);
        for (int y = 0; y < img.height; y++) {
            for (int x = 0; x < img.width; x++) {
                const auto srcOffset = static_cast<size_t>(y) * static_cast<size_t>(paddedWidth) + static_cast<size_t>(x);
                const auto dstOffset = (static_cast<size_t>(y) * static_cast<size_t>(img.width) + static_cast<size_t>(x)) * 4;
                std::memcpy(buffer.data() + dstOffset, &paddedRGBA[srcOffset], 4);
                buffer[dstOffset + 3] = 255;
            }
        }

        return Image{Format::RGBA8888, img.width, img.height, std::move(buffer)};
    };
}

} // namespace

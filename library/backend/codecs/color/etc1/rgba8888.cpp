#include <haio_convert.hpp>

#include <algorithm>
#include <cstring>
#include <limits>
#include <optional>

#include <Decode.hpp>
#include <ProcessRGB.hpp>

namespace {

using Haio::Bytes;

int paddedSize(int size) {
    return (size + 3) & ~3;
}

size_t blockCount(int width, int height) {
    return static_cast<size_t>(paddedSize(width) / 4) * static_cast<size_t>(paddedSize(height) / 4);
}

std::optional<Haio::Error> validateDimensions(int width, int height) {
    if (width <= 0 || height <= 0) {
        return Haio::Error{Haio::ErrorCode::InvalidInput, "etc1 images require positive width and height"};
    }
    return std::nullopt;
}

Haio::Result<std::vector<uint32_t>> makePaddedBGRA(Bytes src, Haio::Size size, int paddedWidth, int paddedHeight) {
    const auto expectedSize = static_cast<size_t>(size.width) * static_cast<size_t>(size.height) * 4;
    if (src.size() != expectedSize) {
        return std::unexpected(Haio::Error{Haio::ErrorCode::InvalidInput, "invalid rgba8888 data size for etc1 encode"});
    }

    std::vector<uint32_t> pixels(static_cast<size_t>(paddedWidth) * static_cast<size_t>(paddedHeight));
    for (int y = 0; y < paddedHeight; y++) {
        const int srcY = std::min(y, size.height - 1);
        for (int x = 0; x < paddedWidth; x++) {
            const int srcX = std::min(x, size.width - 1);
            const auto srcOffset = (static_cast<size_t>(srcY) * static_cast<size_t>(size.width) + static_cast<size_t>(srcX)) * 4;
            const uint8_t bgra[4] = {
                src[srcOffset + 2],
                src[srcOffset + 1],
                src[srcOffset + 0],
                src[srcOffset + 3]
            };
            std::memcpy(&pixels[static_cast<size_t>(y) * static_cast<size_t>(paddedWidth) + static_cast<size_t>(x)], bgra, 4);
        }
    }
    return pixels;
}

}

namespace Haio::Codecs {

/**
 * @addtogroup move
 * @{
 */
template <>
Result<void> Move<Color::ETC1, Color::RGBA8888>(Bytes src, std::span<uint8_t> dst, Size size) {
    const size_t blocks = blockCount(size.width, size.height);
    const size_t expectedSize = blocks * 8;
    if (src.size() != expectedSize) {
        HAIO_FAIL(InvalidInput, "invalid etc1 data size");
    }

    const int paddedWidth = paddedSize(size.width);
    const int paddedHeight = paddedSize(size.height);
    std::vector<uint64_t> compressed(blocks);
    std::memcpy(compressed.data(), src.data(), expectedSize);

    std::vector<uint32_t> paddedRGBA(static_cast<size_t>(paddedWidth) * static_cast<size_t>(paddedHeight));
    DecodeRGB(compressed.data(), paddedRGBA.data(), paddedWidth, paddedHeight);

    std::vector<uint8_t> buffer(static_cast<size_t>(size.width) * static_cast<size_t>(size.height) * 4);
    for (int y = 0; y < size.height; y++) {
        for (int x = 0; x < size.width; x++) {
            const auto srcOffset = static_cast<size_t>(y) * static_cast<size_t>(paddedWidth) + static_cast<size_t>(x);
            const auto dstOffset = (static_cast<size_t>(y) * static_cast<size_t>(size.width) + static_cast<size_t>(x)) * 4;
            std::memcpy(buffer.data() + dstOffset, &paddedRGBA[srcOffset], 4);
            buffer[dstOffset + 3] = 255;
        }
    }


    std::memcpy(dst.data(), buffer.data(), std::min(dst.size(), buffer.size()));
    return {};
}
/** @} */

/**
 * @addtogroup convert
 * @{
 */
template <>
Result<Image<Color::RGBA8888>> Convert<Color::ETC1, Color::RGBA8888>(Image<Color::ETC1> src) {
    return convertVia<Color::ETC1, Color::RGBA8888>(std::move(src));
}
/** @} */

}

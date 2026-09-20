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
/** lossy: four by four blocks, and the dimensions round up to whole blocks */
template <>
Result<void> Move<Color::RGBA8888, Color::ETC1>(Bytes src, std::span<uint8_t> dst, Size size) {
    const int paddedWidth = paddedSize(size.width);
    const int paddedHeight = paddedSize(size.height);
    const size_t blocks = blockCount(size.width, size.height);
    if (blocks > std::numeric_limits<uint32_t>::max()) {
        HAIO_FAIL(InvalidInput, "etc1 image is too large");
    }

    HAIO_TRY(pixels, makePaddedBGRA(src, size, paddedWidth, paddedHeight));
    std::vector<uint64_t> compressed(blocks);

    CompressEtc1RgbDither(
        pixels.data(),
        compressed.data(),
        static_cast<uint32_t>(blocks),
        static_cast<size_t>(paddedWidth)
    );

    std::memcpy(dst.data(), compressed.data(), std::min(dst.size(), blocks * 8));
    return {};
}
/** @} */

/**
 * @addtogroup convert
 * @{
 */
template <>
Result<Image<Color::ETC1>> Convert<Color::RGBA8888, Color::ETC1>(Image<Color::RGBA8888> src) {
    return convertVia<Color::RGBA8888, Color::ETC1>(std::move(src));
}
/** @} */

}

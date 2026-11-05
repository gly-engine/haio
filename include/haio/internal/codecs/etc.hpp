#pragma once

#include <haio_codec.hpp>
#include <haio_util.hpp>

#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

/**
 * the shape every etc and eac conversion has, read once for the eight files that need
 * it: pad out to whole blocks, hand them to etcpak, copy the answer back.
 *
 * it is a header beside the sources rather than a ninth .cpp because the build scans
 * .cpp files for capabilities, and a helper that lived in one of them would be shared
 * by accident. nothing here knows which compressor it is calling: the caller passes
 * one in, which is the only thing that differs between etc1, etc2 and the two eacs.
 */
namespace Haio::Codecs::Etc {

/** one side rounded up to whole blocks, which is the size etcpak is handed */
constexpr int padded(int side) { return (side + 3) & ~3; }

constexpr size_t blocks(Size size) {
    return static_cast<size_t>(padded(size.width) / 4) * static_cast<size_t>(padded(size.height) / 4);
}

/** what every compressor here is: blocks of bgra in, sixty four bit words out */
using Compressor = void (*)(const uint32_t* src, uint64_t* dst, uint32_t blocks, size_t width);

/** and every decoder: those words back into rgba, a whole padded picture at a time */
using Decoder = void (*)(const uint64_t* src, uint32_t* dst, int32_t width, int32_t height);

/**
 * rgba8888 into blocks.
 *
 * etcpak reads bgra and wants whole blocks, so the edge pixels are repeated out into
 * the padding rather than left black: a block that is half picture and half nothing
 * would spend its two colours on the nothing.
 */
inline Result<void> compress(Bytes src, std::span<uint8_t> dst, Size size, Color color, Compressor compressor) {
    const auto pixels = static_cast<size_t>(size.width) * static_cast<size_t>(size.height);
    if (src.size() != pixels * 4) {
        HAIO_FAIL(InvalidInput, "invalid rgba8888 data size for a block compressor");
    }

    const auto count = blocks(size);
    if (count > std::numeric_limits<uint32_t>::max()) {
        HAIO_FAIL(InvalidInput, "image is too large to compress into blocks");
    }

    const int wide = padded(size.width);
    const int tall = padded(size.height);
    std::vector<uint32_t> block(static_cast<size_t>(wide) * static_cast<size_t>(tall));
    for (int y = 0; y < tall; y++) {
        const auto row = static_cast<size_t>(std::min(y, size.height - 1)) * static_cast<size_t>(size.width);
        for (int x = 0; x < wide; x++) {
            const auto at = (row + static_cast<size_t>(std::min(x, size.width - 1))) * 4;
            const uint8_t bgra[4] = {src[at + 2], src[at + 1], src[at + 0], src[at + 3]};
            std::memcpy(&block[static_cast<size_t>(y) * static_cast<size_t>(wide) + static_cast<size_t>(x)], bgra, 4);
        }
    }

    const auto bytes = count * Util::GPU::blockBytes(color);
    std::vector<uint64_t> words(bytes / 8);
    compressor(block.data(), words.data(), static_cast<uint32_t>(count), static_cast<size_t>(wide));

    std::memcpy(dst.data(), words.data(), std::min(dst.size(), bytes));
    return {};
}

/**
 * and back out, cropped to the size that was asked for.
 *
 * opaque is every colour but etc2a: they carry no alpha, and what the decoder leaves
 * in that byte is whatever its table held rather than a picture being see through.
 */
inline Result<void> decompress(Bytes src, std::span<uint8_t> dst, Size size, Color color,
                               Decoder decoder, bool opaque) {
    const auto bytes = blocks(size) * Util::GPU::blockBytes(color);
    if (src.size() != bytes) HAIO_FAIL(InvalidInput, "invalid block payload size");

    const auto pixels = static_cast<size_t>(size.width) * static_cast<size_t>(size.height);
    if (dst.size() < pixels * 4) HAIO_FAIL(InvalidInput, "rgba8888 destination is too small");

    const int wide = padded(size.width);
    const int tall = padded(size.height);
    std::vector<uint64_t> words(bytes / 8);
    std::memcpy(words.data(), src.data(), bytes);

    std::vector<uint32_t> block(static_cast<size_t>(wide) * static_cast<size_t>(tall));
    decoder(words.data(), block.data(), wide, tall);

    for (int y = 0; y < size.height; y++) {
        for (int x = 0; x < size.width; x++) {
            const auto at = (static_cast<size_t>(y) * static_cast<size_t>(size.width) + static_cast<size_t>(x)) * 4;
            std::memcpy(dst.data() + at,
                        &block[static_cast<size_t>(y) * static_cast<size_t>(wide) + static_cast<size_t>(x)], 4);
            if (opaque) dst[at + 3] = 255;
        }
    }
    return {};
}

}

#include <haio/internal/codecs/roms/tic80.hpp>
#include <haio/generated/codec.hpp>

#include <array>
#include <cstring>

namespace {

using Haio::Bytes;

/** sixteen colours of r, g and b; the scn palette comes first and ovr after it */
constexpr size_t paletteBytes = 48;

/** what a cartridge written today is drawn with when it declares no palette */
constexpr uint8_t sweetie16[paletteBytes] = {
    0x1a, 0x1c, 0x2c, 0x5d, 0x27, 0x5d, 0xb1, 0x3e, 0x53, 0xef, 0x7d, 0x57,
    0xff, 0xcd, 0x75, 0xa7, 0xf0, 0x70, 0x38, 0xb7, 0x64, 0x25, 0x71, 0x79,
    0x29, 0x36, 0x6f, 0x3b, 0x5d, 0xc9, 0x41, 0xa6, 0xf6, 0x73, 0xef, 0xf7,
    0xf4, 0xf4, 0xf4, 0x94, 0xb0, 0xc2, 0x56, 0x6c, 0x86, 0x33, 0x3c, 0x57,
};

/** and what one from before 0.70 is, which is the only reason this table is here */
constexpr uint8_t db16[paletteBytes] = {
    0x14, 0x0c, 0x1c, 0x44, 0x24, 0x34, 0x30, 0x34, 0x6d, 0x4e, 0x4a, 0x4e,
    0x85, 0x4c, 0x30, 0x34, 0x65, 0x24, 0xd0, 0x46, 0x48, 0x75, 0x71, 0x61,
    0x59, 0x7d, 0xce, 0xd2, 0x7d, 0x2c, 0x85, 0x95, 0xa1, 0x6d, 0xaa, 0x2c,
    0xd2, 0xaa, 0x99, 0x6d, 0xc2, 0xca, 0xda, 0xd4, 0x5e, 0xde, 0xee, 0xd6,
};

constexpr uint32_t paletteChunk = 12;
constexpr uint32_t defaultChunk = 17;
constexpr uint32_t coverChunk = 3;
constexpr uint32_t spritesChunk = 2;

/** the colours this cartridge draws with, by the order of preference tic-80 uses */
const uint8_t* paletteOf(Bytes data, const std::vector<Haio::Codecs::Tic80::Sector>& sectors) {
    for (const auto& sector : sectors) {
        if (sector.id == paletteChunk && sector.size >= paletteBytes) return data.data() + sector.at;
    }
    for (const auto& sector : sectors) {
        if (sector.id == defaultChunk) return sweetie16;
    }
    return db16;
}

/**
 * four bits a pixel, low nibble first.
 *
 * tic-80 reads its own buffers with tic_tool_peek4, which is
 * (byte >> ((index & 1) << 2)) & 0xf: the even pixel of a pair is the low half of
 * the byte they share, not the high one.
 */
uint8_t indexAt(Bytes data, size_t from, size_t pixel) {
    const auto byte = data[from + pixel / 2];
    return (byte >> ((pixel & 1) * 4)) & 0x0f;
}

/** a chunk shorter than its layout is zero extended, which is what tic-80 does with one */
uint8_t safeIndex(Bytes data, const Haio::Codecs::Tic80::Sector& sector, size_t pixel) {
    return pixel / 2 < sector.size ? indexAt(data, sector.at, pixel) : 0;
}

void paint(std::vector<uint8_t>& into, const uint8_t* palette, uint8_t index) {
    into.push_back(palette[index * 3 + 0]);
    into.push_back(palette[index * 3 + 1]);
    into.push_back(palette[index * 3 + 2]);
    into.push_back(0xff);
}

}

namespace Haio::Codecs::Tic80 {

Result<Image<Color::RGBA8888>> draw(Bytes data, const std::vector<Sector>& sectors, const Sector& wanted) {
    // the old cover is a whole gif rather than a layout of its own, so it is read
    // by the thing that reads gifs instead of being unpacked here
    if (wanted.id == coverChunk) {
        HAIO_TRY(bytes, Util::slice(data, wanted.at, wanted.size));
        const Blob gif{Format::GIF, Color::RGBA8888, "image/gif", {}, std::move(bytes)};
        return Decode<Format::GIF, Color::RGBA8888>(gif);
    }

    const auto* palette = paletteOf(data, sectors);
    std::vector<uint8_t> pixels;

    /**
     * the sprites are not one picture but two hundred and fifty six of them, eight
     * by eight, thirty two bytes each. they are laid out sixteen to a row, which is
     * the sheet every tool that shows them shows.
     */
    if (wanted.id == spritesChunk) {
        pixels.reserve(static_cast<size_t>(sheetSide) * sheetSide * 4);
        for (int y = 0; y < sheetSide; y++) {
            for (int x = 0; x < sheetSide; x++) {
                const auto tile = static_cast<size_t>(y / 8) * 16 + static_cast<size_t>(x / 8);
                const auto within = static_cast<size_t>(y % 8) * 8 + static_cast<size_t>(x % 8);
                paint(pixels, palette, safeIndex(data, wanted, tile * 64 + within));
            }
        }
        return Image<Color::RGBA8888>{sheetSide, sheetSide, std::move(pixels)};
    }

    pixels.reserve(static_cast<size_t>(screenWidth) * screenHeight * 4);
    for (size_t pixel = 0; pixel < static_cast<size_t>(screenWidth) * screenHeight; pixel++) {
        paint(pixels, palette, safeIndex(data, wanted, pixel));
    }
    return Image<Color::RGBA8888>{screenWidth, screenHeight, std::move(pixels)};
}

}

#pragma once

#include <haio/internal/codecs/roms/sectors.hpp>

/**
 * a tic-80 cartridge: a run of chunks, each a four byte header and its bytes, with
 * nothing between them and no magic at the front.
 *
 * byte zero of a header packs the bank into its top three bits and the chunk type
 * into its low five; bytes one and two are the size, little endian; byte three is
 * reserved. a size of zero means a whole bank of sixty four kilobytes, but only for
 * code and binary, which are the two that may fill one.
 */
namespace Haio::Codecs::Tic80 {

using Roms::Sector;

/** the screen every cartridge draws on, and the sheet its sprites are laid out in */
inline constexpr int screenWidth = 240;
inline constexpr int screenHeight = 136;
inline constexpr int sheetSide = 128;

/**
 * whether the bytes are a cartridge.
 *
 * there is nothing at the front to recognise, so what tells one apart is that its
 * chunks tile the file exactly: a run that ends past the end, or short of it, was
 * never a cartridge. it is the same shape of answer a tar gives.
 */
bool isCartridge(Bytes data);

/** every chunk in the file, in the order they sit in it */
Result<std::vector<Sector>> sectorsOf(Bytes data);

/** the chunk a cartridge shows for itself: its screen, or the gif older ones kept */
const Sector* cover(const std::vector<Sector>& sectors);

/** one drawable chunk as pixels, with the colours the cartridge carries */
Result<Image<Color::RGBA8888>> draw(Bytes data, const std::vector<Sector>& sectors, const Sector& wanted);

}

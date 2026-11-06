#pragma once

#include <haio/internal/codecs/roms/sectors.hpp>

namespace Haio::Codecs::Tic80 {

using Roms::Sector;

inline constexpr int screenWidth = 240;
inline constexpr int screenHeight = 136;
inline constexpr int sheetSide = 128;

/** a cartridge carries no magic; what tells one apart is its chunks tiling the file exactly */
bool isCartridge(Bytes data);

Result<std::vector<Sector>> sectorsOf(Bytes data);
const Sector* cover(const std::vector<Sector>& sectors);
Result<Image<Color::RGBA8888>> draw(Bytes data, const std::vector<Sector>& sectors, const Sector& wanted);

}

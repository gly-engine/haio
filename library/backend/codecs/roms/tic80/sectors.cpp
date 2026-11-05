#include <haio/internal/codecs/roms/tic80.hpp>

#include <array>
#include <string_view>

namespace {

/**
 * the chunk types cart.c declares, and whether haio can draw each one.
 *
 * three of twenty one draw. the rest are a cartridge's code, its music, its map and
 * its flags: real parts of the file, with nothing in them that is a picture. they
 * are still listed, so a refusal can say what is there rather than only what is not.
 */
struct Kind {
    std::string_view name;
    bool drawable;
};

constexpr Kind kinds[] = {
    {"CHUNK_DUMMY",        false},
    {"CHUNK_TILES",        false},   // 4bpp like the sprites; a sheet of its own one day
    {"CHUNK_SPRITES",      true },
    {"CHUNK_COVER_DEP",    true },   // a whole gif, from before 0.90
    {"CHUNK_MAP",          false},
    {"CHUNK_CODE",         false},
    {"CHUNK_FLAGS",        false},
    {"CHUNK_TEMP2",        false},
    {"CHUNK_TEMP3",        false},
    {"CHUNK_SAMPLES",      false},
    {"CHUNK_WAVEFORM",     false},
    {"CHUNK_TEMP4",        false},
    {"CHUNK_PALETTE",      false},
    {"CHUNK_PATTERNS_DEP", false},
    {"CHUNK_MUSIC",        false},
    {"CHUNK_PATTERNS",     false},
    {"CHUNK_CODE_ZIP",     false},
    {"CHUNK_DEFAULT",      false},
    {"CHUNK_SCREEN",       true },
    {"CHUNK_BINARY",       false},
    {"CHUNK_LANG",         false},
};

/** only these two may fill a bank, and a full one writes its size down as zero */
constexpr uint32_t code = 5;
constexpr uint32_t binary = 19;

}

namespace Haio::Codecs::Tic80 {

Result<std::vector<Sector>> sectorsOf(Bytes data) {
    std::vector<Sector> sectors;
    size_t at = 0;

    while (at + 4 <= data.size()) {
        Sector sector;
        const uint32_t type = data[at] & 0x1f;
        const uint32_t bank = data[at] >> 5;
        sector.id = type;
        sector.size = static_cast<size_t>(data[at + 1]) | (static_cast<size_t>(data[at + 2]) << 8);
        if (sector.size == 0 && (type == code || type == binary)) sector.size = 65536;
        sector.at = at + 4;

        if (sector.size > data.size() - sector.at) HAIO_FAIL(InvalidInput, "a cartridge chunk runs past the end");

        sector.name = type < std::size(kinds) ? std::string(kinds[type].name)
                                              : "CHUNK_" + std::to_string(type);
        sector.drawable = type < std::size(kinds) && kinds[type].drawable;
        // the first of a type keeps the bare name; a second bank says which it is
        if (bank != 0) sector.name += "." + std::to_string(bank);

        sectors.push_back(std::move(sector));
        at = sectors.back().at + sectors.back().size;
    }

    if (at != data.size() || sectors.empty()) HAIO_FAIL(UnsupportedFormat, "this is not a tic-80 cartridge");
    return sectors;
}

bool isCartridge(Bytes data) {
    return sectorsOf(data).has_value();
}

/**
 * what a cartridge shows for itself.
 *
 * since 0.90 that is bank zero of the screen chunk; before it, a gif in a chunk of
 * its own. a cartridge written today has the first and a cartridge from 2019 has
 * the second, so both are looked for and the newer one wins.
 */
const Sector* cover(const std::vector<Sector>& sectors) {
    if (const auto* screen = Roms::named(sectors, "CHUNK_SCREEN")) return screen;
    return Roms::named(sectors, "CHUNK_COVER_DEP");
}

}

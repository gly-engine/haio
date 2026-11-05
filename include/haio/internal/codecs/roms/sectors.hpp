#pragma once

#include <haio_codec.hpp>

#include <string>
#include <vector>

/**
 * what every cartridge has in common, which is less than it looks: a run of named
 * parts, some of which are pictures.
 *
 * a rom is not a codec and not a transform. it is a file that contains things, and
 * the only question the rest of haio asks it is "what is in here, and which of those
 * can you draw". the answer is read out of the bytes rather than written down,
 * because a tic-80 cartridge names its chunks from a fixed table while a doom wad
 * names its lumps whatever the author felt like.
 */
namespace Haio::Codecs::Roms {

struct Sector {
    /** what the cartridge calls it, which is what -define names */
    std::string name;
    /** what the cartridge numbers it, for the formats that number before they name */
    uint32_t id = 0;
    size_t at = 0;
    size_t size = 0;
    /** whether haio can turn these bytes into a picture; most parts of a rom are not one */
    bool drawable = false;
};

/** the names, for a refusal that says what the file actually holds */
std::string listed(const std::vector<Sector>& sectors);

/** the ones that are pictures, for the refusal when somebody asks for one that is not */
std::string drawable(const std::vector<Sector>& sectors);

/** the one that answers to a name, or nothing */
const Sector* named(const std::vector<Sector>& sectors, std::string_view name);

}

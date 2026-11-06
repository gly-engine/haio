#pragma once

#include <haio_codec.hpp>

#include <string>
#include <vector>

namespace Haio::Codecs::Roms {

struct Sector {
    std::string name;
    uint32_t id = 0;
    size_t at = 0;
    size_t size = 0;
    bool drawable = false;
};

std::string listed(const std::vector<Sector>& sectors);
std::string drawable(const std::vector<Sector>& sectors);
const Sector* named(const std::vector<Sector>& sectors, std::string_view name);

}

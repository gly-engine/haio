#include <haio/internal/codecs/roms/sectors.hpp>

#include <algorithm>

namespace Haio::Codecs::Roms {

std::string listed(const std::vector<Sector>& sectors) {
    std::string names;
    for (const auto& sector : sectors) {
        if (!names.empty()) names += ", ";
        names += sector.name;
    }
    return names;
}

std::string drawable(const std::vector<Sector>& sectors) {
    std::string names;
    for (const auto& sector : sectors) {
        if (!sector.drawable) continue;
        if (!names.empty()) names += ", ";
        names += sector.name;
    }
    return names.empty() ? "none of them" : names;
}

const Sector* named(const std::vector<Sector>& sectors, std::string_view name) {
    const auto found = std::ranges::find_if(sectors, [name](const Sector& sector) { return sector.name == name; });
    return found == sectors.end() ? nullptr : &*found;
}

}

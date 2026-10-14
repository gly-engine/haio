#pragma once

#include "haio/domain.hpp"

#include <meta>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/** the enums are the loops: a new enumerator grows every table on its own */
#define HAIO_FOR_EACH_FORMAT(e) \
    template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^Haio::Format)))

#define HAIO_FOR_EACH_COLOR(e) \
    template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^Haio::Color)))

#define HAIO_FOR_EACH_BRUSH(e) \
    template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^Haio::Implementation::Codecs::Brush)))

namespace Haio::Util {

using namespace Implementation;

/**
 * the names of an enum's enumerators, lowered, for a OptionShape::Name option: a new
 * enumerator is a new name the line accepts, with nothing else to update. there are
 * no aliases on purpose -- one name per thing is the whole point of asking the enum.
 */
template <typename E>
consteval std::span<const char* const> namesOf() {
    // a pointer and not a string_view, because only a structural type can be made static
    std::vector<const char*> out;
    for (const auto e : std::meta::enumerators_of(^^E)) {
        std::string name{std::meta::identifier_of(e)};
        for (auto& c : name) {
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        }
        out.push_back(std::define_static_string(name));
    }
    return std::define_static_array(out);
}

/** the enumerator is RadialGradient, the name people type is radial-gradient */
consteval std::string_view kebabOf(std::meta::info enumerator) {
    std::string out;
    for (const char c : std::meta::identifier_of(enumerator)) {
        if (c >= 'A' && c <= 'Z') {
            if (!out.empty()) out += '-';
            out += static_cast<char>(c - 'A' + 'a');
        } else {
            out += c;
        }
    }
    return std::define_static_string(out);
}

/** the enumerator is PNG, the name people type is png */
consteval std::string_view lowerOf(std::meta::info enumerator) {
    std::string out{std::meta::identifier_of(enumerator)};
    for (auto& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return std::define_static_string(out);
}

}

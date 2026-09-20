#pragma once

#include <haio.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

namespace Haio::Signature {

template <size_t N>
inline bool matches(std::span<const uint8_t> data, const std::array<uint8_t, N>& magic) {
    return data.size() >= N && std::equal(magic.begin(), magic.end(), data.begin());
}

inline bool matches(std::span<const uint8_t> data, std::string_view magic) {
    return data.size() >= magic.size()
        && std::equal(magic.begin(), magic.end(), data.begin(), [](char a, uint8_t b) {
               return static_cast<uint8_t>(a) == b;
           });
}

}

#pragma once

#include "haio_common.hpp"
#include "haio_formats.hpp"

namespace Haio::Buffer {

template<Format From, Format To>
void Copy(const std::vector<uint8_t>& input, std::vector<uint8_t>& output);

}

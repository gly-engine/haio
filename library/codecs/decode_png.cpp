#include "haio.hpp"

namespace Haio {
    template<>
    Stage Decode<Format::PNG>() {
        return [](const Image& img) {
            std::vector<uint8_t> buffer = {0, 1, 2, 3, 4};
            Image out{Format::PNG, 6, 5, buffer};
            return out;
        };
    }
}

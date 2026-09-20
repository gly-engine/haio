#include <haio_util.hpp>

#include <algorithm>

namespace Haio::Util {

bool matches(Bytes data, Bytes magic) {
    return data.size() >= magic.size() && std::equal(magic.begin(), magic.end(), data.begin());
}

bool matches(Bytes data, std::string_view magic) {
    return data.size() >= magic.size()
        && std::equal(magic.begin(), magic.end(), data.begin(), [](char a, uint8_t b) {
               return static_cast<uint8_t>(a) == b;
           });
}

}

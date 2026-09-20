#include "haio.hpp"
#include "signature.hpp"

namespace Haio {

template <>
bool Detect<Format::PPM>(std::span<const uint8_t> data) {
    // the whole netpbm family, although only P6 is written here
    return data.size() >= 3 && data[0] == 'P' && data[1] >= '1' && data[1] <= '6'
        && (data[2] == '\n' || data[2] == '\r' || data[2] == ' ' || data[2] == '\t');
}

template <>
Stage Encode<Format::PPM>() {
    return [](const Image& img) {
        std::string header = "P6\n" + std::to_string(img.width) + " "
                           + std::to_string(img.height) + "\n255\n";
        std::vector<uint8_t> buffer(header.begin(), header.end());

        if (img.type == Format::RGBA8888) {
            buffer.reserve(buffer.size() + img.width * img.height * 3);
            Buffer::Copy<Format::RGBA8888, Format::RGB888>(img.data, buffer);
        } else {
            buffer.insert(buffer.end(), img.data.begin(), img.data.end());
        }

        return Image{Format::PPM, img.width, img.height, std::move(buffer)};
    };
}

} // namespace Haio

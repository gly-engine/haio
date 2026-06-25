#include "haio.hpp"

namespace Haio {

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

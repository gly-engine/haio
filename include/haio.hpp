#pragma once

#include "haio_common.hpp"
#include "haio_formats.hpp"
#include "haio_buffer.hpp"
#include "haio_iwindow.hpp"
#include "haio_pipeline.hpp"

namespace Haio {

struct Image {
    Format type;
    int width;
    int height;
    std::vector<uint8_t> data;
};

using Stage = std::function<Image(const Image&)>;

/**
 * true when the bytes carry this format's signature. it answers a question about
 * bytes instead of transforming an image, so it is not a Stage: probing one blob
 * against every format would copy the buffer once per format.
 */
template<Format F>
bool Detect(std::span<const uint8_t> data);

template<Format F>
Stage Encode();

template<Format F>
Stage Decode();

inline Stage operator|(Stage a, Stage b) {
    return [=](const Image& img) {
        return b(a(img));
    };
}

inline Image operator>>(std::istream& in, Stage decode) {
    std::vector<uint8_t> buffer(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>()
    );
    Image raw{Format::RAW, 0, 0, std::move(buffer)};
    return decode(raw);
}

inline std::ostream& operator>>(const Image& img, std::ostream& out) {
    out.write(reinterpret_cast<const char*>(img.data.data()), img.data.size());
    return out;
}

std::unique_ptr<IWindow> CreateWindow(const char* title, int width, int height);

}

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

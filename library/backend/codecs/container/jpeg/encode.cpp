#include <haio_codec.hpp>
#include <haio_codecs.hpp>
#include <haio_convert.hpp>

#include <turbojpeg.h>

#include <cstddef>
#include <vector>

namespace {

struct Handle {
    tjhandle raw = nullptr;
    ~Handle() { if (raw) tj3Destroy(raw); }
};

}

namespace Haio::Codecs {

/**
 * from yuv planes, which is what a jpeg wants.
 *
 * anything else converts first, and does so where the caller can see it: an encoder
 * that quietly accepted rgb would hide a colour conversion inside what looks like a
 * container change.
 */
template <>
Result<Blob> Encode<Format::JPEG, Color::YUV420>(Image<Color::YUV420> img) {
    const Size size{img.width, img.height};
    HAIO_TRY(expected, sizeOf(Color::YUV420, size));
    if (img.data.size() != expected) HAIO_FAIL(InvalidInput, "invalid yuv420 image for jpeg encode");

    thread_local Handle handle{tj3Init(TJINIT_COMPRESS)};
    if (!handle.raw) HAIO_FAIL(Internal, "cannot start the jpeg encoder");

    tj3Set(handle.raw, TJPARAM_SUBSAMP, TJSAMP_420);
    tj3Set(handle.raw, TJPARAM_QUALITY, 90);
    tj3Set(handle.raw, TJPARAM_NOREALLOC, 1);

    const size_t capacity = tj3JPEGBufSize(img.width, img.height, TJSAMP_420);
    if (capacity == 0) HAIO_FAIL(Internal, "cannot size the jpeg buffer");

    std::vector<uint8_t> buffer;
    buffer.reserve(capacity);

    uint8_t* dst = buffer.data();
    size_t written = capacity;

    if (tj3CompressFromYUV8(handle.raw, img.data.data(), img.width, 1, img.height, &dst, &written) != 0) {
        HAIO_FAIL(Internal, "the jpeg could not be encoded");
    }


    if (dst != buffer.data()) {
        HAIO_FAIL(Internal, "jpeg encoder ignored the buffer we gave it");
    }

    buffer.resize(written);

    return Blob{Format::JPEG, Color::YUV420, "image/jpeg", {}, std::move(buffer)};
}

}
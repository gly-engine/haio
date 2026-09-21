#include <haio_codec.hpp>
#include <haio_codecs.hpp>
#include <haio_convert.hpp>

#include <turbojpeg.h>

namespace {

struct Handle {
    tjhandle raw = nullptr;
    ~Handle() { if (raw) tj3Destroy(raw); }
};

/** what turbojpeg allocates is freed by turbojpeg, whatever happens in between */
struct Owned {
    uint8_t* raw = nullptr;
    ~Owned() { if (raw) tj3Free(raw); }
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

    Handle handle{tj3Init(TJINIT_COMPRESS)};
    if (!handle.raw) HAIO_FAIL(Internal, "cannot start the jpeg encoder");

    tj3Set(handle.raw, TJPARAM_SUBSAMP, TJSAMP_420);
    tj3Set(handle.raw, TJPARAM_QUALITY, 90);

    Owned out;
    size_t written = 0;
    if (tj3CompressFromYUV8(handle.raw, img.data.data(), img.width, 1, img.height, &out.raw, &written) != 0) {
        HAIO_FAIL(Internal, "the jpeg could not be encoded");
    }

    return Blob{Format::JPEG, Color::YUV420, "image/jpeg", {},
                std::vector<uint8_t>(out.raw, out.raw + written)};
}

}

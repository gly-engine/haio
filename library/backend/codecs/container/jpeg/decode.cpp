#include <haio_codec.hpp>
#include <haio_codecs.hpp>
#include <haio_convert.hpp>

#include <turbojpeg.h>

#include <memory>

namespace {

/** the handle owns a decompressor, and there is no path out of here that leaks it */
struct Handle {
    tjhandle raw = nullptr;
    ~Handle() { if (raw) tj3Destroy(raw); }
};

}

namespace Haio::Codecs {

/**
 * straight to yuv planes, which is what the file already holds.
 *
 * a jpeg is yuv all the way down, so a decoder that hands back rgb has both thrown
 * away the chroma layout and spent the work doing it. asking for the planes means the
 * picture arrives as it was stored, and whoever wants rgb converts once, visibly.
 *
 * only 4:2:0 is read. it is what cameras and ad creatives produce, and reading 4:4:4
 * or 4:2:2 into a buffer sized for 4:2:0 would quietly truncate the chroma rather
 * than fail, so those are refused instead.
 */
template <>
Result<Image<Color::YUV420>> Decode<Format::JPEG, Color::YUV420>(const Blob& blob) {
    Handle handle{tj3Init(TJINIT_DECOMPRESS)};
    if (!handle.raw) HAIO_FAIL(Internal, "cannot start the jpeg decoder");

    if (tj3DecompressHeader(handle.raw, blob.data.data(), blob.data.size()) != 0) {
        HAIO_FAIL(InvalidInput, "this is not a jpeg haio can read");
    }

    const int width = tj3Get(handle.raw, TJPARAM_JPEGWIDTH);
    const int height = tj3Get(handle.raw, TJPARAM_JPEGHEIGHT);
    const int sampling = tj3Get(handle.raw, TJPARAM_SUBSAMP);

    if (width <= 0 || height <= 0) HAIO_FAIL(InvalidInput, "the jpeg has no area");
    if (sampling != TJSAMP_420) {
        HAIO_FAIL(UnsupportedFormat, "haio reads 4:2:0 jpegs, and this one is not");
    }
    if (width % 2 != 0 || height % 2 != 0) {
        // 4:2:0 stores chroma for whole two by two blocks, so an odd side means the
        // file's own planes are padded and would not line up with ours
        HAIO_FAIL(UnsupportedFormat, "haio reads 4:2:0 jpegs with even sides, and this one is odd");
    }

    HAIO_TRY(bytes, sizeOf(Color::YUV420, Size{width, height}));
    std::vector<uint8_t> planes(bytes);

    // align 1 means the planes sit back to back with no padding, which is the layout
    // Color::YUV420 is defined as
    if (tj3DecompressToYUV8(handle.raw, blob.data.data(), blob.data.size(), planes.data(), 1) != 0) {
        HAIO_FAIL(InvalidInput, "the jpeg could not be decoded");
    }
    return Image<Color::YUV420>{width, height, std::move(planes)};
}

}

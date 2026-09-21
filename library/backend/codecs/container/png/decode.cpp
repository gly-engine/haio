#include <haio_codec.hpp>

// the modules are compiled once, in backend/codecs/wuffs.cpp. NONMONOLITHIC keeps
// the declarations without them, and IMPLEMENTATION is what completes the types:
// wuffs leaves them opaque otherwise, and a decoder on the stack needs the size
#define WUFFS_NONMONOLITHIC
#define WUFFS_IMPLEMENTATION
#define WUFFS_CONFIG__MODULES
#include <wuffs-v0.4.c>

namespace {

/**
 * the pixel format wuffs is asked for, and how many bytes that costs per pixel.
 *
 * wuffs offers both channel orders and its names say which is which, so the only way
 * to get this wrong is to pick the wrong one: asking for BGR where Color::RGB888 is
 * meant hands back a picture with red and blue swapped, and every stage after it
 * copies three bytes faithfully without ever noticing.
 */
struct Target { uint32_t pixelFormat; size_t stride; };

constexpr Target targetFor(Haio::Color colour) {
    return colour == Haio::Color::RGB888
        ? Target{WUFFS_BASE__PIXEL_FORMAT__RGB, 3}
        : Target{WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL, 4};
}

}

namespace Haio::Codecs {

namespace {

/** one decode for every colour png can hand back; the caller says which it wants */
template <Color P>
Result<Image<P>> decodePng(const Blob& blob) {
    wuffs_png__decoder dec;
    wuffs_base__status st = wuffs_png__decoder__initialize(&dec, sizeof dec, WUFFS_VERSION, WUFFS_INITIALIZE__DEFAULT_OPTIONS);

    if (!wuffs_base__status__is_ok(&st)) {
        HAIO_FAIL(InvalidInput, "wuffs is not ok");
    }

    wuffs_base__io_buffer src = wuffs_base__ptr_u8__reader((uint8_t*)blob.data.data(), blob.data.size(), true);

    wuffs_base__image_config cfg;
    memset(&cfg, 0, sizeof cfg);
    st = wuffs_base__image_decoder__decode_image_config(wuffs_png__decoder__upcast_as__wuffs_base__image_decoder(&dec), &cfg, &src);
    if (!wuffs_base__status__is_ok(&st)) {
        HAIO_FAIL(InvalidInput, "invalid png config");
    }

    uint32_t w = wuffs_base__pixel_config__width(&cfg.pixcfg);
    uint32_t h = wuffs_base__pixel_config__height(&cfg.pixcfg);
    if (!w || !h) {
        HAIO_FAIL(InvalidInput, "invalid png dimensions");
    }

    constexpr auto target = targetFor(P);
    wuffs_base__pixel_config__set(&cfg.pixcfg, target.pixelFormat, WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, w, h);

    std::vector<uint8_t> buffer(static_cast<size_t>(w) * static_cast<size_t>(h) * target.stride);

    wuffs_base__pixel_buffer pb;
    st = wuffs_base__pixel_buffer__set_from_slice(&pb, &cfg.pixcfg, wuffs_base__make_slice_u8(buffer.data(), buffer.size()));
    if (!wuffs_base__status__is_ok(&st)) {
        HAIO_FAIL(InvalidInput, "failed to create png pixel buffer");
    }

    wuffs_base__range_ii_u64 wb = wuffs_base__image_decoder__workbuf_len(wuffs_png__decoder__upcast_as__wuffs_base__image_decoder(&dec));
    std::vector<uint8_t> work(wb.max_incl ? static_cast<size_t>(wb.max_incl) : 0);

    st = wuffs_base__image_decoder__decode_frame(
        wuffs_png__decoder__upcast_as__wuffs_base__image_decoder(&dec),
        &pb,
        &src,
        WUFFS_BASE__PIXEL_BLEND__SRC,
        wuffs_base__make_slice_u8(work.data(), work.size()),
        nullptr
    );
    if (!wuffs_base__status__is_ok(&st)) {
        HAIO_FAIL(InvalidInput, "failed to decode png frame");
    }


    return Image<P>{static_cast<int>(w), static_cast<int>(h), std::move(buffer)};
}

}

/**
 * @addtogroup decode
 * @{
 */
template <>
Result<Image<Color::RGBA8888>> Decode<Format::PNG, Color::RGBA8888>(const Blob& blob) {
    return decodePng<Color::RGBA8888>(blob);
}
/** @} */

/**
 * @addtogroup decode
 * @{
 */
/** a truecolour png without alpha: kept at three bytes instead of inflated to four */
template <>
Result<Image<Color::RGB888>> Decode<Format::PNG, Color::RGB888>(const Blob& blob) {
    return decodePng<Color::RGB888>(blob);
}
/** @} */

}

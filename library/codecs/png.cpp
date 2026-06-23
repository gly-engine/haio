#include <haio.hpp>

#include <spng.h>

#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string_view>

#define WUFFS_IMPLEMENTATION
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__PNG
#define WUFFS_CONFIG__MODULE__ZLIB
#define WUFFS_CONFIG__MODULE__DEFLATE
#define WUFFS_CONFIG__MODULE__ADLER32
#define WUFFS_CONFIG__MODULE__CRC32
#include <wuffs-v0.4.c>

namespace {

void checkSpng(int err, std::string_view message) {
    if (err) throw std::runtime_error(std::string(message) + ": " + spng_strerror(err));
}

}

namespace Haio {

template <>
Stage Decode<Format::PNG>() {
    return [](const Image& img) {
        wuffs_png__decoder dec;
        wuffs_base__status st = wuffs_png__decoder__initialize(&dec, sizeof dec, WUFFS_VERSION, WUFFS_INITIALIZE__DEFAULT_OPTIONS);

        if (!wuffs_base__status__is_ok(&st)) {
            throw std::runtime_error("wuffs is not ok");
        }

        wuffs_base__io_buffer src = wuffs_base__ptr_u8__reader((uint8_t*)img.data.data(), img.data.size(), true);

        wuffs_base__image_config cfg;
        memset(&cfg, 0, sizeof cfg);
        st = wuffs_base__image_decoder__decode_image_config(wuffs_png__decoder__upcast_as__wuffs_base__image_decoder(&dec), &cfg, &src);
        if (!wuffs_base__status__is_ok(&st)) {
            throw std::runtime_error("invalid png config");
        }

        uint32_t w = wuffs_base__pixel_config__width(&cfg.pixcfg);
        uint32_t h = wuffs_base__pixel_config__height(&cfg.pixcfg);
        if (!w || !h) {
            throw std::runtime_error("invalid png dimensions");
        }

        wuffs_base__pixel_config__set(&cfg.pixcfg, WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL, WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, w, h);

        std::vector<uint8_t> buffer(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);

        wuffs_base__pixel_buffer pb;
        st = wuffs_base__pixel_buffer__set_from_slice(&pb, &cfg.pixcfg, wuffs_base__make_slice_u8(buffer.data(), buffer.size()));
        if (!wuffs_base__status__is_ok(&st)) {
            throw std::runtime_error("failed to create png pixel buffer");
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
            throw std::runtime_error("failed to decode png frame");
        }

        return Image{Format::RGBA8888, static_cast<int>(w), static_cast<int>(h), std::move(buffer)};
    };
}

template <>
Stage Encode<Format::PNG>() {
    return [](const Image& img) {
        if (img.type != Format::RGBA8888) {
            throw std::runtime_error("png encode only accepts rgba8888 images");
        }
        if (img.width <= 0 || img.height <= 0 || img.data.size() != static_cast<size_t>(img.width) * static_cast<size_t>(img.height) * 4) {
            throw std::runtime_error("invalid rgba8888 image for png encode");
        }

        std::unique_ptr<spng_ctx, decltype(&spng_ctx_free)> ctx(spng_ctx_new(SPNG_CTX_ENCODER), spng_ctx_free);
        if (!ctx) throw std::runtime_error("failed to create png writer");

        checkSpng(spng_set_option(ctx.get(), SPNG_ENCODE_TO_BUFFER, 1), "failed to configure png writer");

        spng_ihdr ihdr{};
        ihdr.width = static_cast<uint32_t>(img.width);
        ihdr.height = static_cast<uint32_t>(img.height);
        ihdr.bit_depth = 8;
        ihdr.color_type = SPNG_COLOR_TYPE_TRUECOLOR_ALPHA;
        ihdr.compression_method = 0;
        ihdr.filter_method = 0;
        ihdr.interlace_method = SPNG_INTERLACE_NONE;

        checkSpng(spng_set_ihdr(ctx.get(), &ihdr), "failed to set png header");
        checkSpng(spng_encode_image(ctx.get(), img.data.data(), img.data.size(), SPNG_FMT_PNG, SPNG_ENCODE_FINALIZE), "failed to encode png");

        size_t size = 0;
        int err = 0;
        std::unique_ptr<void, decltype(&std::free)> bytes(spng_get_png_buffer(ctx.get(), &size, &err), std::free);
        checkSpng(err, "failed to read encoded png");
        if (!bytes) throw std::runtime_error("failed to read encoded png");

        const auto* begin = static_cast<const uint8_t*>(bytes.get());
        std::vector<uint8_t> buffer(begin, begin + size);
        return Image{Format::PNG, img.width, img.height, std::move(buffer)};
    };
}

}

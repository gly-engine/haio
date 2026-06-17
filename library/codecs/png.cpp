#include <haio.hpp>

#include <png.h>

#include <cstring>
#include <stdexcept>

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

struct PngWriteTarget {
    std::vector<uint8_t>* data;
};

void writePngBytes(png_structp png, png_bytep bytes, png_size_t len) {
    auto* target = static_cast<PngWriteTarget*>(png_get_io_ptr(png));
    target->data->insert(target->data->end(), bytes, bytes + len);
}

void flushPngBytes(png_structp) {}

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

        png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        if (!png) throw std::runtime_error("failed to create png writer");

        png_infop info = png_create_info_struct(png);
        if (!info) {
            png_destroy_write_struct(&png, nullptr);
            throw std::runtime_error("failed to create png info");
        }

        std::vector<uint8_t> buffer;
        PngWriteTarget target{&buffer};

        if (setjmp(png_jmpbuf(png))) {
            png_destroy_write_struct(&png, &info);
            throw std::runtime_error("failed to encode png");
        }

        png_set_write_fn(png, &target, writePngBytes, flushPngBytes);
        png_set_IHDR(png, info, img.width, img.height, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
        png_write_info(png, info);

        std::vector<png_bytep> rows(static_cast<size_t>(img.height));
        for (int y = 0; y < img.height; y++) {
            rows[static_cast<size_t>(y)] = const_cast<png_bytep>(img.data.data() + static_cast<size_t>(y) * static_cast<size_t>(img.width) * 4);
        }
        png_write_image(png, rows.data());
        png_write_end(png, nullptr);
        png_destroy_write_struct(&png, &info);

        return Image{Format::PNG, img.width, img.height, std::move(buffer)};
    };
}

}

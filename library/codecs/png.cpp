#include <haio.hpp>

#define WUFFS_IMPLEMENTATION
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__PNG
#define WUFFS_CONFIG__MODULE__ZLIB
#define WUFFS_CONFIG__MODULE__DEFLATE
#define WUFFS_CONFIG__MODULE__ADLER32
#define WUFFS_CONFIG__MODULE__CRC32
#include <wuffs-v0.4.c>

namespace Haio {
    template <>
    Stage Decode<Format::PNG>() {
        return [](const Image &img) {
            wuffs_png__decoder dec;
            wuffs_base__status st = wuffs_png__decoder__initialize(&dec, sizeof dec, WUFFS_VERSION, WUFFS_INITIALIZE__DEFAULT_OPTIONS);

            if (!wuffs_base__status__is_ok(&st)) {
                throw std::runtime_error("wuffs is not ok!");
            }

            wuffs_base__io_buffer src = wuffs_base__ptr_u8__reader((uint8_t *)img.data.data(), img.data.size(), true);

            wuffs_base__image_config cfg;
            memset(&cfg, 0, sizeof cfg);
            st = wuffs_base__image_decoder__decode_image_config(wuffs_png__decoder__upcast_as__wuffs_base__image_decoder(&dec), &cfg, &src);
            if (!wuffs_base__status__is_ok(&st)) {
                throw std::runtime_error("invalid png config");
            }

            uint32_t w = wuffs_base__pixel_config__width(&cfg.pixcfg);
            uint32_t h = wuffs_base__pixel_config__height(&cfg.pixcfg);
            if (!w || !h) {
                throw std::runtime_error("invalid width ou height");
            }

            wuffs_base__pixel_config__set(&cfg.pixcfg, WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL, WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, w, h);

            size_t img_size = (size_t)w * (size_t)h * 4;
            std::vector<uint8_t> buffer(img_size);

            wuffs_base__pixel_buffer pb;
            st = wuffs_base__pixel_buffer__set_from_slice(&pb, &cfg.pixcfg, wuffs_base__make_slice_u8(buffer.data(), buffer.size()));

            if (!wuffs_base__status__is_ok(&st)) {
                throw std::runtime_error("error 2");
            }

            wuffs_base__range_ii_u64 wb = wuffs_base__image_decoder__workbuf_len(wuffs_png__decoder__upcast_as__wuffs_base__image_decoder(&dec));
            std::vector<uint8_t> work(wb.max_incl ? (size_t)wb.max_incl : 0);

            st = wuffs_base__image_decoder__decode_frame(wuffs_png__decoder__upcast_as__wuffs_base__image_decoder(&dec), &pb, &src,
                                                        WUFFS_BASE__PIXEL_BLEND__SRC, wuffs_base__make_slice_u8(work.data(), work.size()), NULL);
            if (!wuffs_base__status__is_ok(&st)) {
                throw std::runtime_error("error 3");
            }

            return Image{Format::RGBA8888, (int)w, (int)h, std::move(buffer)};
        };
    }

    template <>
    Stage Encode<Format::PNG>() {
        return [](const Image &img) {
            std::vector<uint8_t> buffer = {0, 1, 2, 3, 4};
            Image out{Format::PNG, 6, 5, buffer};
            return out;
        };
    }
}

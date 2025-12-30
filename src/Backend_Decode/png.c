#include <spng.h>

#include "img.h"
/*
bitmap_t *DecodePngBufferToInternal(const void *png_data, size_t png_size) {
    spng_ctx *ctx = spng_ctx_new(0);
    if (!ctx) return NULL;

    if (spng_set_png_buffer(ctx, (void*)png_data, png_size) != 0) {
        spng_ctx_free(ctx);
        return NULL;
    }

    struct spng_ihdr ihdr;
    if (spng_get_ihdr(ctx, &ihdr) != 0) {
        spng_ctx_free(ctx);
        return NULL;
    }

    size_t out_size;
    if (spng_decoded_image_size(ctx, SPNG_FMT_RGBA8, &out_size) != 0) {
        spng_ctx_free(ctx);
        return NULL;
    }

    bitmap_t *bmp = malloc(sizeof(bitmap_t) + out_size);
    if (!bmp) {
        spng_ctx_free(ctx);
        return NULL;
    }

    if (spng_decode_image(ctx, bmp->pixels, out_size, SPNG_FMT_RGBA8, SPNG_DECODE_TRNS) != 0) {
        free(bmp);
        spng_ctx_free(ctx);
        return NULL;
    }

    bmp->width = (int16_t)ihdr.width;
    bmp->height = (int16_t)ihdr.height;
    bmp->buffer_len = out_size;

    spng_ctx_free(ctx);
    return bmp;
}
*/
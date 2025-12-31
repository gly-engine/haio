#include <string.h>
#include <assert.h>

#include <spng.h>

#include "img.h"

void DecodePngBufferToRGBA8888(raio_worker_handle_t *handle, raio_buffer_t *src, raio_buffer_t *dst) {
    spng_ctx *ctx = (spng_ctx*) handle->ctx;

    do {
        if (handle->done) {
            break;
        }

        if (!ctx) {
            handle->ctx = spng_ctx_new(0);
            ctx = (spng_ctx*) handle->ctx;
        }

        if (!ctx) {
            assert(false);
        }

        int ret_buffer = spng_set_png_buffer(ctx, src->data.u8, src->len);
        if (ret_buffer != 0) {
            assert(false);
        }

        if (!dst->data.u8) {
            struct spng_ihdr ihdr;
            spng_get_ihdr(ctx, &ihdr);
            size_t row_size = ihdr.width * 4;
            handle->width = ihdr.width;
            handle->height = ihdr.height;
            handle->buffer_aux.len = row_size;
            handle->buffer_aux.capacity = row_size;
            handle->buffer_aux.data.u8 = malloc(row_size);
            dst->len = 0;
            dst->capacity = row_size;
            dst->data.u8 = malloc(row_size);
        }

        printf("linhas lidas %d/%d\n", handle->lines, handle->height);
        if (handle->lines < handle->height) {
            dst->pos = 0;
            int ret;
            size_t bytes_read = handle->width * 4;
            while ((ret = spng_decode_scanline(ctx, handle->buffer_aux.data.u8, handle->width)) == 0) {
                printf("nova linha\n");
                if (dst->pos + bytes_read > dst->capacity) {
                    size_t new_capacity = (dst->capacity == 0) ? bytes_read * 2 : dst->capacity * 2;
                    uint8_t *new_buf = realloc(dst->data.u8, new_capacity);
                    if (!new_buf) {
                        assert(false);
                    }
                    dst->data.u8 = new_buf;
                    dst->capacity = new_capacity;
                }

                memcpy(dst->data.u8 + dst->pos, handle->buffer_aux.data.u8, bytes_read);
                dst->pos += bytes_read;
                handle->lines++;
            }
            printf("ret: %d %s\n", ret, spng_strerror(ret));
            dst->len = dst->pos;

            if (ret == SPNG_EINVAL || ret == SPNG_EMEM || ret == SPNG_EIO) {
                assert(false);
            }
            break;
        }

        handle->ctx = NULL;
        handle->done = true;
        spng_ctx_free(ctx);
        if (dst->data.u8) free(dst->data.u8);
        if (handle->buffer_aux.data.u8) free(handle->buffer_aux.data.u8);
    }
    while(0);
}

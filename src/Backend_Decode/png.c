#include <string.h>
#include <assert.h>

#include <spng.h>

#include "img.h"

void DecodePngBufferToRGBA8(raio_worker_handle_t *handle, raio_buffer_t *src, raio_buffer_t *dst) {
    int ret;
    spng_ctx *ctx = (spng_ctx*) handle->ctx;

    do {
        printf("state png %d\n", handle->state);
        
        if (handle->state == RAIO_FSM_WORKER_DONE) {
            printf("png done!\n");
            break;
        }

        if (handle-> state == RAIO_FSM_WORKER_FINISHING) {
            handle->state = RAIO_FSM_WORKER_DONE;
            break;
        }

        if (!ctx) {
            handle->ctx = spng_ctx_new(0);
            ctx = (spng_ctx*) handle->ctx;
        }

        if (!ctx) {
            assert(false);
        }

        if ((ret = spng_set_png_buffer(ctx, src->data.u8, src->len)) != 0) {
            printf("error: %s\n", spng_strerror(ret));
            assert(false);
        }

        if (handle->state == RAIO_FSM_WORKER_NEW) {
            struct spng_ihdr ihdr;
            spng_get_ihdr(ctx, &ihdr);
            size_t row_size = ihdr.width * 4;
            handle->width = ihdr.width;
            handle->height = ihdr.height;
            handle->buffer_aux.len = row_size;
            handle->buffer_aux.size = row_size;
            handle->buffer_aux.data.u8 = malloc(row_size);
            handle->state = RAIO_FSM_WORKER_RUNNING;
            printf("ihr: %dx%d\n", ihdr.width, ihdr.height);
            if((ret = spng_decode_image(ctx, NULL, 0, SPNG_FMT_RGBA8, SPNG_DECODE_PROGRESSIVE)) != 0) {
                printf("spng_encode_image() error: %s\n", spng_strerror(ret));
            }
        }

        dst->len = 0;
        size_t bytes_read = handle->width * 4;
        do {
            ret = spng_decode_row(ctx, handle->buffer_aux.data.u8, bytes_read);
            if (dst->len + bytes_read > dst->size) {
                size_t new_size = (dst->size == 0) ? bytes_read * 2 : dst->size * 2;
                uint8_t *new_buf = realloc(dst->data.u8, new_size);
                if (!new_buf) {
                    assert(false);
                }
                dst->data.u8 = new_buf;
                dst->size = new_size;
            }

            memcpy(dst->data.u8 + dst->len, handle->buffer_aux.data.u8, bytes_read);
            handle->progress.lines.count++;
            dst->len += bytes_read;
        }
        while(ret == 0);
        printf("linhas lidas %d/%d | %ld/%ld\n", handle->progress.lines.count, handle->height, dst->len, dst->size);
        printf("ret: %d %s\n", ret, spng_strerror(ret));

        if (ret == SPNG_EINVAL || ret == SPNG_EMEM || ret == SPNG_EIO) {
            printf("error: %s\n", spng_strerror(ret));
            assert(false);
        }

        
        if (handle->progress.lines.count >= handle->height) {
            handle->state = RAIO_FSM_WORKER_FINISHING;
        }
    }
    while(0);
}

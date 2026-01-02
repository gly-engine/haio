#include <string.h>
#include <assert.h>

#include <spng.h>

#include "raio.h"

void DecodePngBufferToRGBA8(raio_worker_handle_t *handle, raio_buffer_t *src, raio_buffer_t *dst) {
    spng_ctx *ctx = (spng_ctx*) handle->ctx;
    int ret;

    do {        
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

        if (handle->state == RAIO_FSM_WORKER_NEW) {
            if ((ret = spng_set_png_buffer(ctx, src->data.u8, src->len)) != 0) {
                printf("error: %s\n", spng_strerror(ret));
                assert(false);
            }
            struct spng_ihdr ihdr;
            spng_get_ihdr(ctx, &ihdr);
            size_t row_size = ihdr.width * 4;
            handle->width = ihdr.width;
            handle->height = ihdr.height;
            handle->state = RAIO_FSM_WORKER_RUNNING;
            if((ret = spng_decode_image(ctx, NULL, 0, SPNG_FMT_RGBA8, SPNG_DECODE_PROGRESSIVE)) != 0) {
                printf("spng_encode_image() error: %s\n", spng_strerror(ret));
            }
        }

        size_t bytes_read = handle->width * 4;

        BufferEnsureCapacity(dst, bytes_read);
        ret = spng_decode_row(ctx, dst->data.ptr, bytes_read);
        handle->progress.lines.count++;
        dst->len = bytes_read;

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

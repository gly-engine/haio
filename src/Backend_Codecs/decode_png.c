#include <string.h>
#include <assert.h>

#include <spng.h>

#include "haio.h"
#include "haio/functions.h"

void DecodePngBufferToRGBA8(haio_worker_handle_t *handle, haio_buffer_t *src, haio_buffer_t *dst) {
    spng_ctx *ctx = (spng_ctx*) handle->ctx;
    int ret;

    do {        
        if (handle->state == HAIO_FSM_WORKER_DONE) {
            break;
        }

        if (handle->state == HAIO_FSM_WORKER_FINISHING) {
            spng_ctx_free(ctx);
            BufferClose(&handle->aux);
            handle->state = HAIO_FSM_WORKER_DONE;
            break;
        }

        if (handle->state == HAIO_FSM_WORKER_NEW) {
            if (src->len == 0) {
                break;
            }
            ctx = spng_ctx_new(0);
            BufferPush(&handle->aux, src->data.str, src->len);

            assert(ctx);
            if ((ret = spng_set_png_buffer(ctx, handle->aux.data.u8, handle->aux.len)) != 0) {
                fprintf(stderr, "error: %s\n", spng_strerror(ret));
                return;
            }
            struct spng_ihdr ihdr;
            spng_get_ihdr(ctx, &ihdr);
            handle->ctx = ctx;
            handle->state = HAIO_FSM_WORKER_RUNNING;
            handle->canvas.width = (uint16_t) ihdr.width;
            handle->canvas.height = (uint16_t) ihdr.height;
            if((ret = spng_decode_image(ctx, NULL, 0, SPNG_FMT_RGBA8, SPNG_DECODE_PROGRESSIVE)) != 0) {
                printf("spng_encode_image() error: %s\n", spng_strerror(ret));
            }
        }

        assert(ctx);

        size_t bytes_read = handle->canvas.width * 4;

        BufferEnsureCapacity(dst, bytes_read);
        ret = spng_decode_row(ctx, dst->data.ptr, bytes_read);
        handle->progress.lines.count++;
        dst->len = bytes_read;

        if (ret == SPNG_EINVAL || ret == SPNG_EMEM || ret == SPNG_EIO) {
            printf("error: %s\n", spng_strerror(ret));
            return;
        }

        
        if (handle->progress.lines.count >= handle->canvas.height) {
            handle->state = HAIO_FSM_WORKER_FINISHING;
        }
    }
    while(0);
}

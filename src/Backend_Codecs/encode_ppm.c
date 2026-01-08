#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "haio.h"
#include "haio/functions.h"

/**
 * @pre needs @c handle.width and @c handle.height
 * @param [in,out] handle
 * @param [in] src
 * @param [out] dst
 */
void EncodeRGBA8ToPPM(haio_worker_handle_t *handle, haio_buffer_t *src, haio_buffer_t *dst)
{
    // @todo move??
    dst->len = 0;

    do {
        if (handle->state == HAIO_FSM_WORKER_DONE) {
            break;
        }

        if (handle->state == HAIO_FSM_WORKER_FINISHING) {
            handle->state = HAIO_FSM_WORKER_DONE;
            break;
        }

        if (handle->state == HAIO_FSM_WORKER_NEW) {
            if (src->len == 0) break;
            if (handle->canvas.parent->width == 0 || handle->canvas.parent->height == 0) {
                assert(false);
            }

            BufferEnsureCapacity(dst, 32);

            handle->canvas.width = handle->canvas.parent->width;
            handle->canvas.height = handle->canvas.parent->height;
            handle->progress.nbytes.count = 0;
            handle->progress.nbytes.total = (handle->canvas.width * handle->canvas.height) * 3lu;
            dst->len += (size_t) snprintf(dst->data.str, dst->size, "P6\n%d %d\n255\n", handle->canvas.width, handle->canvas.height);
            handle->state = HAIO_FSM_WORKER_RUNNING;
        }

        if (src->len % 4 != 0) {
            assert(false);
        }

        size_t nbytes_rgb = ((src->len / 4) * 3);
        BufferEnsureCapacity(dst, dst->len + nbytes_rgb);
        uint8_t *dst_ptr = &dst->data.u8[dst->len];
        uint8_t *dst_end = dst_ptr + nbytes_rgb;
        uint8_t *src_ptr = src->data.u8;
        uint8_t channel = 0;

        handle->progress.nbytes.count += nbytes_rgb;
        dst->len += nbytes_rgb;

        // @todo otimze with SSSE3
        while (dst_ptr < dst_end) {
            if ((channel++ & 3) != 3) {
                *dst_ptr++ = *src_ptr++;
            } else {
                src_ptr++;
            }
        }

        if (handle->progress.nbytes.count >= handle->progress.nbytes.total) {
            handle->state = HAIO_FSM_WORKER_FINISHING;
        }
    } while (0);
}

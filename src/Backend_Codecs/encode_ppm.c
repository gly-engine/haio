#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "raio.h"
#include "raio/functions.h"

/**
 * @pre needs @c handle.width and @c handle.height
 * @param [in,out] handle
 * @param [in] src
 * @param [out] dst
 */
void EncodeRGBA8ToPPM(raio_worker_handle_t *handle, raio_buffer_t *src, raio_buffer_t *dst)
{
    // @todo move??
    dst->len = 0;

    do {
        if (handle->state == RAIO_FSM_WORKER_DONE) {
            break;
        }

        if (handle->state == RAIO_FSM_WORKER_FINISHING) {
            handle->state = RAIO_FSM_WORKER_DONE;
            break;
        }

        if (handle->state == RAIO_FSM_WORKER_NEW) {
            if (handle->height == 0 || handle->width == 0) {
                assert(false);
            }

            BufferEnsureCapacity(dst, 32);

            handle->progress.nbytes.count = 0;
            handle->progress.nbytes.total = (handle->width * handle->height) * 3;
            dst->len += snprintf(dst->data.str, dst->size, "P6\n%d %d\n255\n", handle->width, handle->height);
            handle->state = RAIO_FSM_WORKER_RUNNING;
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
            handle->state = RAIO_FSM_WORKER_FINISHING;
        }
    } while (0);
}

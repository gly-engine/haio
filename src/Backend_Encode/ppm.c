#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "img.h"

/**
 * @pre needs @c handle.width and @c handle.height
 * @param [in,out] handle
 * @param [in] src
 * @param [out] dst
 */
void EncodeRGBAToPPMBuffer(raio_worker_handle_t *handle, raio_buffer_t *src, raio_buffer_t *dst)
{

    do {
        if (handle->state == RAIO_FSM_WORKER_DONE) {
            break;
        }

        if (handle->state == RAIO_FSM_WORKER_FINISHING) {
            handle->state = RAIO_FSM_WORKER_DONE;
            break;
        }

        // @todo move this to always reset dest
        dst->len = 0;

        if (handle->state == RAIO_FSM_WORKER_NEW) {
            if (handle->height == 0 || handle->width == 0) {
                assert(false);
            }

            dst->size = 1024;
            dst->data.str = malloc(1024);
            assert(dst->data.str);
            //if (dst->size < 32) {
            //    assert(false);
            //}

            handle->progress.nbytes.count = 0;
            handle->progress.nbytes.total = handle->width * handle->height * 3;
            dst->len += snprintf(dst->data.str, dst->size, "P6\n%d %d\n255\n", handle->width, handle->height);
        }

        if (src->len % 4 != 0) {
            assert(false);
        }

        size_t nbytes_rgb = ((src->len / 4) * 3);
        uint8_t *dst_ptr = &dst->data.u8[dst->len];
        uint8_t *dst_end = dst_ptr + nbytes_rgb;
        uint8_t *src_ptr = src->data.u8;
        uint8_t channel = 0;

        dst->len += nbytes_rgb;
        handle->progress.nbytes.count += nbytes_rgb;

        if (dst->len > dst->size) {
            uint8_t *new_buf = realloc(dst->data.u8, dst->len);
            if (!new_buf) {
                assert(false);
            }
            dst->data.u8 = new_buf;
        }

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

#include <string.h>
#include <assert.h>

#include "haio.h"
#include "haio/functions.h"

/** @todo rewrite this */
void FilterRGBA8Crop(haio_worker_handle_t *handle, haio_buffer_t *src, haio_buffer_t *dst) {
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

            handle->canvas.width = 80;
            handle->canvas.height = 80;

            // Calculando offsets para centralizar o crop
            //uint16_t src_w = handle->canvas.parent->width;
            //uint16_t src_h = handle->canvas.parent->height;
            handle->canvas.offset_x = 0;
            handle->canvas.offset_y = 0;

            handle->progress.nbytes.count = 0;
            handle->progress.nbytes.total = (handle->canvas.width * handle->canvas.height) * 4lu; // RGBA

            handle->state = HAIO_FSM_WORKER_RUNNING;
        }

        if (src->len % 4 != 0) {
            assert(false);
        }

        size_t src_w = handle->canvas.parent->width;
        size_t crop_w = handle->canvas.width;
        size_t crop_h = handle->canvas.height;
        size_t offset_x = handle->canvas.offset_x;
        size_t offset_y = handle->canvas.offset_y;

        BufferEnsureCapacity(dst, crop_w * crop_h * 4);

        uint8_t *dst_ptr = dst->data.u8;
        uint8_t *src_ptr = src->data.u8 + (offset_y * src_w + offset_x) * 4;

        for (size_t y = 0; y < crop_h; y++) {
            memcpy(dst_ptr, src_ptr, crop_w * 4);
            dst_ptr += crop_w * 4;
            src_ptr += src_w * 4;
        }

        dst->len += crop_w * crop_h * 4;
        handle->progress.nbytes.count += crop_w * crop_h * 4;

        handle->state = HAIO_FSM_WORKER_FINISHING;
    }
    while(0);
}
#include <string.h>
#include <assert.h>

#include "haio.h"
#include "haio/functions.h"

void FilterRGBA8Crop(haio_worker_handle_t *handle, haio_buffer_t *src, haio_buffer_t *dst)
{
    do {
        if (handle->state == HAIO_FSM_WORKER_DONE)
            break;

        if (handle->state == HAIO_FSM_WORKER_FINISHING) {
            handle->state = HAIO_FSM_WORKER_DONE;
            break;
        }

        if (handle->state == HAIO_FSM_WORKER_NEW) {
            if (src->len == 0)
                break;

            if (!handle->canvas.parent ||
                handle->canvas.parent->width  == 0 ||
                handle->canvas.parent->height == 0) {
                assert(false);
            }

            handle->canvas.width  = 200;
            handle->canvas.height = 200;
            handle->canvas.offset_x = 100;
            handle->canvas.offset_y = 100;

            handle->progress.nbytes.count = 0;
            handle->progress.nbytes.total =
                handle->canvas.parent->width *
                handle->canvas.parent->height * 4lu;

            handle->state = HAIO_FSM_WORKER_RUNNING;
        }

        assert(src->len % 4 == 0);

        uint8_t *p = src->data.ptr;
        size_t remaining = src->len;
        size_t base = handle->progress.nbytes.count;

        uint16_t src_w = handle->canvas.parent->width;
        size_t local = 0;

        while (remaining) {
            size_t abs_pixel = (base + local) >> 2;

            size_t sx = abs_pixel % src_w;
            size_t sy = abs_pixel / src_w;

            uint16_t y = (uint16_t)sy;

            size_t pixels_left = src_w - sx;
            size_t bytes_left  = pixels_left * 4;

            size_t chunk = remaining < bytes_left? remaining: bytes_left;

            if (chunk == 0) chunk = 4;

            if (y >= handle->canvas.offset_y &&
                y <  handle->canvas.offset_y + handle->canvas.height) {

                size_t cx0 = handle->canvas.offset_x;
                size_t cx1 = cx0 + handle->canvas.width;

                size_t lx0 = sx;
                size_t lx1 = lx0 + (chunk >> 2);

                size_t copy_x0 = lx0 > cx0 ? lx0 : cx0;
                size_t copy_x1 = lx1 < cx1 ? lx1 : cx1;

                if (copy_x1 > copy_x0) {
                    size_t skip = (copy_x0 - lx0) * 4;
                    size_t size = (copy_x1 - copy_x0) * 4;
                    BufferPush(dst, p + local + skip, size);
                }
            }

            local     += chunk;
            remaining -= chunk;
        }

        handle->progress.nbytes.count += src->len;
        if (handle->progress.nbytes.count >= handle->progress.nbytes.total) {
            handle->state = HAIO_FSM_WORKER_FINISHING;
        }
    } while (0);
}

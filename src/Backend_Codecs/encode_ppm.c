#include <assert.h>

#include "haio.h"
#include "haio/functions.h"

/**
 * @param [in,out] handle
 * @param [in] src
 * @param [out] dst
 */
void EncodeRGBA8ToPPM(haio_worker_handle_t *handle, haio_buffer_t *src, haio_buffer_t *dst)
{
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
            handle->canvas.width = handle->canvas.parent->width;
            handle->canvas.height = handle->canvas.parent->height;
            handle->progress.nbytes.count = 0;
            handle->progress.nbytes.total = (handle->canvas.width * handle->canvas.height) * 3lu;
            BufferPushString(dst, "P6\n%d %d\n255\n", handle->canvas.width, handle->canvas.height);
            handle->state = HAIO_FSM_WORKER_RUNNING;
        }

        handle->progress.nbytes.count += ((src->len / 4) * 3);
        if(BufferPush43(dst, src->data.ptr, src->len)) {
            assert(false);
        }

        if (handle->progress.nbytes.count >= handle->progress.nbytes.total) {
            handle->state = HAIO_FSM_WORKER_FINISHING;
        }
    } while (0);
}

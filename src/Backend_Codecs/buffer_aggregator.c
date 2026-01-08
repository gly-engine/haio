#include "haio.h"
#include "haio/functions.h"

void BufferAggregatorUntilZero(haio_worker_handle_t *handle, haio_buffer_t *src, haio_buffer_t *dst) {
    do {
        if (handle->state == HAIO_FSM_WORKER_NEW) {
            handle->state = HAIO_FSM_WORKER_RUNNING;
        }       

        if (handle->state == HAIO_FSM_WORKER_FINISHING) {
            handle->state = HAIO_FSM_WORKER_DONE;
            BufferMove(&handle->aux, dst);
            break;
        }

        if (handle->state == HAIO_FSM_WORKER_DONE) {
            BufferClose(&handle->aux);
            break;
        }

        if (src->len == 0) {
            handle-> state = HAIO_FSM_WORKER_FINISHING;
            break;
        }

        BufferPush(&handle->aux, src->data.ptr, src->len);        
    }
    while(0);
}

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "haio.h"
#include "haio/functions.h"

void BufferAggregatorUntilZero(haio_worker_handle_t *handle, haio_buffer_t *src, haio_buffer_t *dst) {
    haio_buffer_t *aux = (haio_buffer_t*) handle->usr.ctx;

    do {
        if (handle->state == HAIO_FSM_WORKER_NEW) {
            /** @todo unecessary malloc? */
            aux = malloc(sizeof(haio_buffer_t));
            memset(aux, 0, sizeof(haio_buffer_t));
            handle->usr.ctx = aux;
        }

        if (handle->state == HAIO_FSM_WORKER_DONE) {
            break;
        }

        assert(aux);

        if (handle-> state == HAIO_FSM_WORKER_FINISHING) {
            handle->state = HAIO_FSM_WORKER_DONE;
            BufferMove(aux, dst);
            free(aux);
            handle->usr.ctx = NULL;
            break;
        }

        if (handle->state == HAIO_FSM_WORKER_NEW) {
            handle->state = HAIO_FSM_WORKER_RUNNING;
            BufferReset(aux);
        }

        if (src->len == 0) {
            handle-> state = HAIO_FSM_WORKER_FINISHING;
            break;
        }

        BufferPush(aux, src->data.ptr, src->len);        
    }
    while(0);
}

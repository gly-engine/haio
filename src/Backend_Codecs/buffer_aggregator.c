#include <stdlib.h>
#include <assert.h>

#include "raio.h"

void BufferAggregatorUntilZero(raio_worker_handle_t *handle, raio_buffer_t *src, raio_buffer_t *dst) {
    raio_buffer_t *aux = (raio_buffer_t*) handle->ctx;

    do {
        if (!aux) {
            aux = calloc(1, sizeof(raio_buffer_t));
            handle->ctx = aux;
            assert(aux);
        }

        if (handle->state == RAIO_FSM_WORKER_DONE) {
            break;
        }

        if (handle-> state == RAIO_FSM_WORKER_FINISHING) {
            handle->state = RAIO_FSM_WORKER_DONE;
            BufferMove(aux, dst);
            free(aux);
            handle->ctx = NULL;
            break;
        }

        if (handle->state == RAIO_FSM_WORKER_NEW) {
            handle->state = RAIO_FSM_WORKER_RUNNING;
            BufferReset(aux);
        }

        if (src->len == 0) {
            handle-> state = RAIO_FSM_WORKER_FINISHING;
            break;
        }

        BufferPush(aux, src->data.ptr, src->len);        
    }
    while(0);
}

/**
 * @file pipeline.c
 * @author RodrigoDornelles
 * @date 2026-01-03
 */

#include "haio.h"
#include "haio/functions.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>

static char output_buffer_error[] = "pipeline output buffer failed";
static char pipeline_route_error[] = "pipeline has no workers";

static void compact_buffer(haio_buffer_t *buffer) {
    size_t remaining = buffer->len - buffer->pos;
    memmove(buffer->data.u8, buffer->data.u8 + buffer->pos, remaining);
    buffer->len = remaining;
    buffer->pos = 0;
}

static size_t write_output(
    haio_pipeline_t *pipe,
    haio_buffer_t *buffer,
    char *dst,
    size_t dst_len
) {
    bool queued = BufferHasQeue(pipe->aux_buf_out);
    if (queued
        && pipe->aux_buf_out->len + buffer->len > pipe->aux_buf_out->size) {
        compact_buffer(pipe->aux_buf_out);
    }
    if (queued || buffer->len > dst_len) {
        if (buffer->len > 0
            && BufferPush(pipe->aux_buf_out, buffer->data.str, buffer->len) != 0) {
            pipe->error = output_buffer_error;
            return 0;
        }
        return BufferPop(pipe->aux_buf_out, dst, dst_len);
    }
    if (buffer->len > 0) {
        memcpy(dst, buffer->data.str, buffer->len);
    }
    return buffer->len;
}

/**
 */
void PipelineBegin(haio_pipeline_t* pipe, haio_type_t first_step) {
    memset(pipe, 0, sizeof(haio_pipeline_t));
    pipe->current_format = first_step;
    pipe->state = HAIO_FSM_PIPE_PREPARE;
}

/**
 * @details add tasks to streaming pipeline
 * @param [in,out] pipe
 */
void PipelineStepAdd(haio_pipeline_t* pipe, haio_type_t next_step) {
    haio_worker_func_t workers[HAIO_MAX_STEPS_BY_WORKER];
    haio_type_t from = pipe->current_format;
    haio_type_t to = next_step;
    haio_type_t result = HAIO_TYPE_NULL;

    uint8_t nworkers = GetCodecWorkersFromFormats(from, to, workers, HAIO_MAX_STEPS_BY_WORKER, &result);
    pipe->current_format = result;
    pipe->state = HAIO_FSM_PIPE_PREPARE;

    memcpy(&pipe->workers[pipe->worker_count], workers, nworkers * sizeof(haio_worker_func_t));
    pipe->worker_count += nworkers;
}

/**
 * @details finish pipeline tasks
 * @param [in,out] pipe
 */
void PipelineEnd(haio_pipeline_t* pipe, haio_type_t last_step) {
    PipelineStepAdd(pipe, last_step);
    pipe->state = HAIO_FSM_PIPE_RUNNING;
    for (uint8_t i = 1; i < pipe->worker_count; i++) {
        pipe->handlers[i].canvas.parent = &pipe->handlers[i - 1].canvas;
    }
}

/**
 * @details streaming and process
 * @param [in,out] pipe
 * @param [in] src_ptr input buffer
 * @param [in] src_len input lenght
 * @param [out] dst_ptr output buffer
 * @param [in] dst_len output length
 *
 * @note @c src_ptr and @c dst_ptr is safety to be a same pointer.
 *
 * @return number of bytes read
 * @retval 0 when process is finish
 * @retval 0 when has error
 */
size_t PipelineProcess(haio_pipeline_t *pipe, char* src_ptr, size_t src_len, char* dst_ptr, size_t dst_len) {
    //haio_worker_fsm_t last_state;
    size_t nbytes = 0;

    if (pipe->worker_count == 0) {
        pipe->error = pipeline_route_error;
        return 0;
    }

    haio_buffer_t buffer_input = {
        .data.str = src_ptr,
        .size = src_len,
        .len = src_len
    };

    BufferReset(&pipe->buffers[1]);
    pipe->workers[0](&pipe->handlers[0], &buffer_input, &pipe->buffers[1]);
    if (pipe->handlers[0].error) {
        pipe->error = pipe->handlers[0].error;
        return 0;
    }
    bool is_done = pipe->handlers[0].state == HAIO_FSM_WORKER_DONE;

    //printf("\n%-32s[%d/%d] (src: %-6ld dst: %-6ld)\n", GetCodecWorkerName(pipe->workers[0]), 1, pipe->worker_count, buffer_input.len, pipe->buffers[1].len);
    for(uint8_t id = 1; id < pipe->worker_count; id++) {
        uint8_t bf1 = id & 1;
        uint8_t bf2 = bf1 ^ 1;
        BufferReset(&pipe->buffers[bf2]);
        pipe->workers[id](&pipe->handlers[id], &pipe->buffers[bf1], &pipe->buffers[bf2]);
        if (pipe->handlers[id].error) {
            pipe->error = pipe->handlers[id].error;
            return 0;
        }
        //printf("%-32s[%d/%d] (src: %-6ld dst: %-6ld) %d %d\n", GetCodecWorkerName(pipe->workers[id]), id+1,  pipe->worker_count, pipe->buffers[bf1].len, pipe->buffers[bf2].len, bf1, bf2);
        is_done &= pipe->handlers[id].state == HAIO_FSM_WORKER_DONE;

    }
    nbytes = write_output(
        pipe,
        &pipe->buffers[pipe->worker_count & 1u],
        dst_ptr,
        dst_len
    );
    if (pipe->error) return 0;

    if (is_done) {
        pipe->state = HAIO_FSM_PIPE_DONE;
    }

    if (BufferHasQeue(pipe->aux_buf_out)) {
        pipe->state = HAIO_FSM_PIPE_RUNNING;
    }

    return nbytes;
}

bool PipelineIsRunning(haio_pipeline_t *pipe) {
    if (PipelineHasError(pipe)) return false;
    return pipe->state == HAIO_FSM_PIPE_RUNNING;
}

/**
 * @details clean internal buffers and queues
 * @param [in,out] pipe
 *
 * @warning should be called @c free(pipe) after the clean
 */
void PipelineClean(haio_pipeline_t *pipe) {
    for (size_t index = 0; index < 2u; index++) {
        BufferClose(&pipe->buffers[index]);
        BufferClose(&pipe->aux_buf_out[index]);
    }
    for (uint8_t index = 0; index < pipe->worker_count; index++) {
        BufferClose(&pipe->handlers[index].aux);
    }
}

bool PipelineHasError(haio_pipeline_t *pipe) {
    return pipe->error != NULL;
}

/**
 * @details check if pipeline has errors
 * @return string null terminted with error message
 * @retval NULL when no has error
 */
char* GetPipelineError(haio_pipeline_t *pipe) {
    return pipe->error;
}

#include <assert.h>
#include <string.h>

#include "haio.h"
#include "haio/functions.h"

uint8_t GetCodecWorkersFromFormats(
    haio_type_t from,
    haio_type_t to,
    haio_worker_func_t *workers,
    uint8_t size,
    haio_type_t *output
) {
    (void) from;
    (void) to;
    (void) workers;
    (void) size;
    (void) output;
    return 0u;
}

static void copy(haio_worker_handle_t *handle, haio_buffer_t *src, haio_buffer_t *dst) {
    int error = BufferPush(dst, src->data.ptr, src->len);
    assert(error == 0);
    (void) error;
    handle->state = src->len == 0 ? HAIO_FSM_WORKER_DONE : HAIO_FSM_WORKER_RUNNING;
}

static void emit(haio_worker_handle_t *handle, haio_buffer_t *src, haio_buffer_t *dst) {
    (void) src;
    if (handle->state == HAIO_FSM_WORKER_DONE) {
        dst->len = 0;
        return;
    }
    size_t size = handle->progress.nbytes.count == 0u ? 5000u : 4000u;
    int byte = handle->progress.nbytes.count == 0u ? 'A' : 'B';
    int error = BufferEnsureCapacity(dst, size);
    assert(error == 0);
    (void) error;
    memset(dst->data.ptr, byte, size);
    dst->len = size;
    handle->progress.nbytes.count++;
    handle->state = handle->progress.nbytes.count == 2u
        ? HAIO_FSM_WORKER_DONE
        : HAIO_FSM_WORKER_RUNNING;
}

static void single(haio_worker_handle_t *handle, haio_buffer_t *src, haio_buffer_t *dst) {
    (void) src;
    if (handle->state == HAIO_FSM_WORKER_DONE) {
        dst->len = 0;
        return;
    }
    int error = BufferPush(dst, "ok", 2u);
    assert(error == 0);
    (void) error;
    handle->state = HAIO_FSM_WORKER_DONE;
}

int main(void) {
    haio_pipeline_t pipe;
    PipelineBegin(&pipe, HAIO_TYPE_BUFFER);
    pipe.workers[0] = copy;
    pipe.workers[1] = copy;
    pipe.workers[2] = emit;
    pipe.worker_count = 3u;
    pipe.state = HAIO_FSM_PIPE_RUNNING;

    char input = 0;
    char output[4096];
    size_t first = PipelineProcess(&pipe, &input, 1u, output, sizeof(output));
    assert(first == sizeof(output));
    for (size_t index = 0; index < first; index++) {
        assert(output[index] == 'A');
    }

    size_t second = PipelineProcess(&pipe, &input, 1u, output, sizeof(output));
    assert(second == sizeof(output));
    for (size_t index = 0; index < 904u; index++) {
        assert(output[index] == 'A');
    }
    for (size_t index = 904u; index < second; index++) {
        assert(output[index] == 'B');
    }
    assert(PipelineIsRunning(&pipe));

    size_t third = PipelineProcess(&pipe, &input, 0u, output, sizeof(output));
    assert(third == 808u);
    for (size_t index = 0; index < third; index++) {
        assert(output[index] == 'B');
    }
    assert(!PipelineIsRunning(&pipe));
    assert(pipe.aux_buf_out[0].size <= 8224u);
    PipelineClean(&pipe);

    haio_pipeline_t direct;
    PipelineBegin(&direct, HAIO_TYPE_IMG_RGBA8);
    direct.workers[0] = single;
    direct.worker_count = 1u;
    direct.state = HAIO_FSM_PIPE_RUNNING;
    size_t direct_size = PipelineProcess(&direct, &input, 0u, output, sizeof(output));
    assert(direct_size == 2u);
    assert(memcmp(output, "ok", 2u) == 0);
    assert(!PipelineIsRunning(&direct));
    PipelineClean(&direct);

    haio_pipeline_t two_workers;
    PipelineBegin(&two_workers, HAIO_TYPE_BUFFER);
    two_workers.workers[0] = copy;
    two_workers.workers[1] = copy;
    two_workers.worker_count = 2u;
    two_workers.state = HAIO_FSM_PIPE_RUNNING;
    size_t first_two = PipelineProcess(&two_workers, &input, 1u, output, sizeof(output));
    size_t second_two = PipelineProcess(&two_workers, &input, 1u, output, sizeof(output));
    assert(first_two == 1u && second_two == 1u);
    PipelineClean(&two_workers);
    return 0;
}

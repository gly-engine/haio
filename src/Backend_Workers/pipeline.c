/**
 * @file pipeline.c
 * @author RodrigoDornelles
 * @date 2026-01-03
 */
#include "raio.h"

/**
 */
void PipelineBegin(raio_pipeline_t* pipe, raio_type_t first_step) {
    pipe->current_format = first_step;
    pipe->state = RAIO_FSM_PIPE_PREPARE;
}

/**
 * @details add tasks to streaming pipeline
 * @param [in,out] pipe
 */
void PipelineStepAdd(raio_pipeline_t* pipe, raio_type_t next_step) {
    raio_worker_func_t workers[10];
    raio_type_t from = pipe->current_format;
    raio_type_t to = next_step;
    raio_type_t result = RAIO_TYPE_NULL;

    int nworkers = GetCodecWorkersFromFormats(from, to, NULL, 0, &result);
    pipe->current_format = result;
    pipe->state = RAIO_FSM_PIPE_PREPARE;

    printf("%d -> %d = %d (%d)\n", from, to, result, nworkers);
    printf("%s -> %s = %s", GetCodecFormatName(from), GetCodecFormatName(to), GetCodecFormatName(result));
}

/**
 * @details finish pipeline tasks
 * @param [in,out] pipe
 */
void PipelineEnd(raio_pipeline_t* pipe, raio_type_t last_step) {
    PipelineStepAdd(pipe, last_step);
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
size_t PipelineProcess(raio_pipeline_t *pipe, char* src_ptr, size_t src_len, char* dst_ptr, size_t dst_len) {

}

bool PipelineIsRunning(raio_pipeline_t *pipe) {
    return false;
}

/**
 * @details clean internal buffers and queues
 * @param [in,out] pipe
 *
 * @warning should be called @c free(pipe) after the clean
 */
void PipelineClean(raio_pipeline_t *pipe) {

}

bool PipelineHasError(raio_pipeline_t *pipe) {
    return false;
}

/**
 * @details check if pipeline has errors
 * @return string null terminted with error message
 * @retval NULL when no has error
 */
char* GetPipelineError(raio_pipeline_t *pipe) {

}

// @todo refact
const raio_worker_t *GetPipelineWorker(raio_type_t from, raio_type_t to) {
    return NULL;
}

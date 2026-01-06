#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    RAIO_TYPE_NULL,
    RAIO_TYPE_BUFFER,
    RAIO_TYPE_BUFFER_FULL,
    RAIO_TYPE_BUFFER_GZIP,
    RAIO_TYPE_URL_FILE,
    RAIO_TYPE_IMG_PNG,
    RAIO_TYPE_IMG_PPM,
    RAIO_TYPE_IMG_BMP,
    RAIO_TYPE_IMG_YUV420,
    RAIO_TYPE_IMG_Y4M420,
    RAIO_TYPE_IMG_RGBA8,
    RAIO_TYPE_FILTER_CROP,
    RAIO_TYPE_COUNT
} raio_type_t;

typedef enum {
    RAIO_FSM_WORKER_NEW,
    RAIO_FSM_WORKER_RUNNING,
    RAIO_FSM_WORKER_FINISHING,
    RAIO_FSM_WORKER_DONE,
    RAIO_FSM_WORKER_IDLE
} raio_worker_fsm_t;

typedef enum {
    RAIO_FSM_PIPE_NEW,
    RAIO_FSM_PIPE_PREPARE
} raio_pipe_fsm_t;

typedef struct {
    size_t len;
    size_t size;
    union {
        void *ptr;
        char *str;
        uint8_t *u8;
    } data;
} raio_buffer_t;

typedef struct {
    void* ctx;
    union {
        struct {
            uint16_t count;
        } lines;
        struct {
            size_t total;
            size_t count;
        } nbytes;
    } progress;
    uint16_t width;
    uint16_t height;
    raio_worker_fsm_t state;
} raio_worker_handle_t;

typedef void (*raio_worker_func_t)(raio_worker_handle_t *handle, raio_buffer_t *src, raio_buffer_t *dst);

typedef struct {
    raio_type_t steps[3];
    raio_worker_func_t func;
} raio_worker_t;

typedef struct {
    char *error;
    raio_pipe_fsm_t state;
    raio_type_t mime_format;
    raio_type_t current_format;
    raio_worker_func_t* funcs;
} raio_pipeline_t;

// Utils:
char* GetExtensionFromString(char *const txt);
raio_type_t GetFormatFromExtension(char *const txt);
// Frontends:
int FrontendConvertCli(int argc, char* argv[]);
int FrontendServerCli(int argc, char* argv[]);
// Backends:
uint8_t GetCodecWorkersFromFormats(raio_type_t from, raio_type_t to, raio_worker_func_t * func_list, uint8_t size,  raio_type_t *output);
const char *const GetCodecWorkerName(raio_worker_func_t func);
const char *const GetCodecFormatName(raio_type_t format);
void PipelineBegin(raio_pipeline_t* pipe, raio_type_t first_step);
void PipelineStepAdd(raio_pipeline_t* pipe, raio_type_t next_step);
void PipelineEnd(raio_pipeline_t* pipe, raio_type_t last_step);
size_t PipelineProcess(raio_pipeline_t *pipe, char* src_ptr, size_t src_len, char* dst_ptr, size_t dst_len);
bool PipelineIsRunning(raio_pipeline_t *pipe);
bool PipelineHasError(raio_pipeline_t *pipe);
void PipelineClean(raio_pipeline_t *pipe);
char* GetPipelineError(raio_pipeline_t *pipe);
// Drivers:
int BufferPush(raio_buffer_t *buf, const void *data, size_t len);
int BufferEnsureCapacity(raio_buffer_t *buf, size_t len);
int BufferReset(raio_buffer_t *buf);
int BufferMove(raio_buffer_t *src, raio_buffer_t *dst);

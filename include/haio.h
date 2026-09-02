#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef HAIO_MAX_STEPS_BY_WORKER
#define HAIO_MAX_STEPS_BY_WORKER (5u)
#endif

#ifndef HAIO_MAX_WORKERS_BY_PIPE
#define HAIO_MAX_WORKERS_BY_PIPE (HAIO_MAX_STEPS_BY_WORKER * 5)
#endif

typedef enum {
    HAIO_TYPE_NULL,
    HAIO_TYPE_BUFFER,
    HAIO_TYPE_BUFFER_FULL,
    HAIO_TYPE_BUFFER_GZIP,
    HAIO_TYPE_URL_FILE,
    HAIO_TYPE_IMG_PNG,
    HAIO_TYPE_IMG_PPM,
    HAIO_TYPE_IMG_BMP,
    HAIO_TYPE_IMG_YUV420,
    HAIO_TYPE_IMG_Y4M420,
    HAIO_TYPE_IMG_ZCIS,
    HAIO_TYPE_IMG_RGBA8,
    HAIO_TYPE_FILTER_CROP,
    HAIO_TYPE_COUNT
} haio_type_t;

typedef enum {
    HAIO_FSM_WORKER_NEW,
    HAIO_FSM_WORKER_RUNNING,
    HAIO_FSM_WORKER_FINISHING,
    HAIO_FSM_WORKER_DONE
} haio_worker_fsm_t;

typedef enum {
    HAIO_FSM_PIPE_NEW,
    HAIO_FSM_PIPE_PREPARE,
    HAIO_FSM_PIPE_RUNNING,
    HAIO_FSM_PIPE_DONE
} haio_pipe_fsm_t;

typedef struct {
    size_t pos;
    size_t len;
    size_t size;
    union {
        void *ptr;
        char *str;
        uint8_t *u8;
    } data;
} haio_buffer_t;

typedef struct haio_canvas_s {
    uint16_t width;
    uint16_t height;
    uint16_t offset_x;
    uint16_t offset_y;
    const struct haio_canvas_s *parent;
} haio_canvas_t;

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
    haio_buffer_t aux;
    haio_canvas_t canvas;
    haio_worker_fsm_t state;
    char *error;
} haio_worker_handle_t;

typedef void (*haio_worker_func_t)(haio_worker_handle_t *handle, haio_buffer_t *src, haio_buffer_t *dst);

typedef struct {
    haio_type_t steps[3];
    haio_worker_func_t func;
} haio_worker_t;

typedef struct {
    char *error;
    uint8_t worker_count;
    haio_pipe_fsm_t state;
    haio_type_t mime_format;
    haio_type_t current_format;
    haio_buffer_t buffers[2];
    haio_buffer_t aux_buf_out[2];
    haio_worker_func_t workers[HAIO_MAX_WORKERS_BY_PIPE];
    haio_worker_handle_t handlers[HAIO_MAX_WORKERS_BY_PIPE];
} haio_pipeline_t;

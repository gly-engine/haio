#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    RAIO_TYPE_NULL,
    RAIO_TYPE_BUFFER,
    RAIO_TYPE_URL_FILE,
    RAIO_TYPE_IMG_PNG,
    RAIO_TYPE_IMG_PPM,
    RAIO_TYPE_IMG_Y4M420,
    RAIO_TYPE_IMG_RGBA8
} raio_type_t;

typedef enum {
    RAIO_FSM_WORKER_NEW,
    RAIO_FSM_WORKER_RUNNING,
    RAIO_FSM_WORKER_FINISHING,
    RAIO_FSM_WORKER_DONE,
    RAIO_FSM_WORKER_IDLE
} raio_worker_fsm_t;

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
    raio_type_t src;
    raio_type_t dst;
    raio_worker_func_t func;
} raio_worker_t;

typedef struct raio_step_s {
    const raio_worker_t *worker;
    struct raio_step_s *next;
} raio_step_t;

typedef struct {
    raio_step_t *head;
    raio_step_t *tail;
} raio_pipeline_t;

// Utils:
char* GetExtensionFromString(char *const txt);
raio_type_t GetFormatFromExtension(char *const txt);
// Frontends:
int FrontendConvertCli(int argc, char* argv[]);
int FrontendServerCli(int argc, char* argv[]);
// Backends:
const raio_worker_t *GetPipelineWorker(raio_type_t from, raio_type_t to);
raio_pipeline_t *BackendPipelineFromUrl(char *const url);
void BackendPipelineToUrl(raio_pipeline_t *pipe, char *const url);
// Drivers:
int BufferPush(raio_buffer_t *buf, const void *data, size_t len);
int BufferEnsureCapacity(raio_buffer_t *buf, size_t len);

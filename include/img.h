#include <stdint.h>
#include <stddef.h>

typedef struct raio_step_s raio_step_t; 

typedef enum {
    RAIO_TYPE_NULL,
    RAIO_TYPE_BUFFER,
    RAIO_TYPE_URL_FILE,
    RAIO_TYPE_IMG_PNG,
    RAIO_TYPE_IMG_PPM,
    RAIO_TYPE_IMG_Y4M_420,
    RAIO_TYPE_IMG_ARGB_8888
} raio_type_t;

typedef struct {
    raio_type_t src;
    raio_type_t dst;
    void* func;
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

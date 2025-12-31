#include <stddef.h>

#include "img.h"

void DecodePngBufferToRGBA8888(raio_worker_handle_t *handle, raio_buffer_t *src, raio_buffer_t *dst);
void EncodeRGBAToPPMBuffer(raio_worker_handle_t *handle, raio_buffer_t *src, raio_buffer_t *dst);

static const raio_worker_t workers[] = {
    {
        .src = RAIO_TYPE_URL_FILE,
        .dst = RAIO_TYPE_BUFFER,
        .func = NULL
    },
    {
        .src = RAIO_TYPE_IMG_PNG,
        .dst = RAIO_TYPE_IMG_ARGB_8888,
        .func = DecodePngBufferToRGBA8888
    },
    {
        .src = RAIO_TYPE_IMG_ARGB_8888,
        .dst = RAIO_TYPE_IMG_PPM,
        .func = EncodeRGBAToPPMBuffer
    },
    {
        .src = RAIO_TYPE_IMG_ARGB_8888,
        .dst = RAIO_TYPE_IMG_Y4M_420,
        .func = NULL
    }
};

static const size_t workers_len = sizeof(workers)/sizeof(raio_worker_t);

const raio_worker_t *GetPipelineWorker(raio_type_t src, raio_type_t dst)
{
    for(size_t i = 0; i < workers_len; i++) {
        if (workers[i].src == src && workers[i].dst == dst) {
            return &workers[i];
        }
    }

    return NULL;
}

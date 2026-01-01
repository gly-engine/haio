#include <stddef.h>

#include "img.h"

void BufferAggregatorUntilZero(raio_worker_handle_t *handle, raio_buffer_t *src, raio_buffer_t *dst);
void DecodePngBufferToRGBA8(raio_worker_handle_t *handle, raio_buffer_t *src, raio_buffer_t *dst);
void EncodeRGBA8ToPPM(raio_worker_handle_t *handle, raio_buffer_t *src, raio_buffer_t *dst);
//void EncodeRGBA8ToYUV(raio_worker_handle_t *handle, raio_buffer_t *src, raio_buffer_t *dst);
//void EncodeYUVToY4M(raio_worker_handle_t *handle, raio_buffer_t *src, raio_buffer_t *dst);

static const raio_worker_t workers[] = {
    {
        .src = RAIO_TYPE_BUFFER,
        .dst = RAIO_TYPE_IMG_PNG,
        .func = BufferAggregatorUntilZero
    },
    {
        .src = RAIO_TYPE_IMG_PNG,
        .dst = RAIO_TYPE_IMG_RGBA8,
        .func = DecodePngBufferToRGBA8
    },
    {
        .src = RAIO_TYPE_IMG_RGBA8,
        .dst = RAIO_TYPE_IMG_PPM,
        .func = EncodeRGBA8ToPPM
    },
    /*{
        .src = RAIO_TYPE_IMG_RGBA8,
        .dst = RAIO_TYPE_IMG_YUV,
        .func = EncodeRGBA8ToYUV
    }
    {
        .src = RAIO_TYPE_IMG_YUV,
        .dst = RAIO_TYPE_IMG_Y4M,
        .func = EncodeY4mToY4M
    }*/
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

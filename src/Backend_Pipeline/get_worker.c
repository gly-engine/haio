#include <stddef.h>

#include "img.h"

//bitmap_t *DecodePngBufferToInternal(const void *png_data, size_t png_size);
//uint8_t *EncodeInternalToPPM(bitmap_t *bmp, size_t *out_size);
//uint8_t *EncodeInternalToY4M(bitmap_t *bmp, size_t *out_size);

static const raio_worker_t workers[] = {
    {
        .src = RAIO_TYPE_URL_FILE,
        .dst = RAIO_TYPE_BUFFER,
        .func = NULL
    },
    {
        .src = RAIO_TYPE_BUFFER,
        .dst = RAIO_TYPE_IMG_PNG,
        .func = NULL
    },
    {
        .src = RAIO_TYPE_IMG_PNG,
        .dst = RAIO_TYPE_IMG_ARGB_8888,
        .func = NULL
    },
    {
        .src = RAIO_TYPE_IMG_ARGB_8888,
        .dst = RAIO_TYPE_IMG_PPM,
        .func = NULL
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
    for(size_t i; i < workers_len; i++) {
        if (workers[i].src == src && workers[i].dst == dst) {
            return &workers[i];
        }
    }

    return NULL;
}

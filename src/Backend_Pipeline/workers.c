#include <stddef.h>

#include "img.h"

// buffers:
void BufferAggregatorUntilZero(raio_worker_handle_t *handle, raio_buffer_t *src, raio_buffer_t *dst);
// conversors:
void ConvertRGBA8ToYUV420(raio_worker_handle_t *handle, raio_buffer_t *src, raio_buffer_t *dst);
// decoders:
void DecodePngBufferToRGBA8(raio_worker_handle_t *handle, raio_buffer_t *src, raio_buffer_t *dst);
// encoders:
void EncodeRGBA8ToPPM(raio_worker_handle_t *handle, raio_buffer_t *src, raio_buffer_t *dst);
void EncodeYUV420ToY4M(raio_worker_handle_t *handle, raio_buffer_t *src, raio_buffer_t *dst);

static const raio_worker_t workers[] = {
    {
        .verts = { RAIO_TYPE_BUFFER, RAIO_TYPE_BUFFER_FULL },
        .func = BufferAggregatorUntilZero
    },
    {
        .verts = { RAIO_TYPE_BUFFER_FULL, RAIO_TYPE_IMG_PNG, RAIO_TYPE_IMG_RGBA8 },
        .func = DecodePngBufferToRGBA8
    },
    {
        .verts = { RAIO_TYPE_IMG_RGBA8, RAIO_TYPE_IMG_PPM },
        .func = EncodeRGBA8ToPPM
    },
    {
        .verts = { RAIO_TYPE_IMG_RGBA8, RAIO_TYPE_IMG_YUV420 },
        .func = ConvertRGBA8ToYUV420
    },
    {
        .verts = { RAIO_TYPE_IMG_YUV420, RAIO_TYPE_IMG_Y4M420 },
        .func = EncodeYUV420ToY4M
    }
};

static const size_t workers_len = sizeof(workers)/sizeof(raio_worker_t);

// faca funcoes privadas de patch find (letra minucusla e statica)
// e tambem faça com que cada add step ou end, ele ja vai procurando o melhor caminho

raio_pipeline_t* PipelineBegin(raio_type_t first_step) {
    // implement
    return NULL;
}


// add step to queue
void PipelineStepAdd(raio_pipeline_t* pipe, raio_type_t next_step) {
    
}

// return 0 if a complete patch find!
int PipelineEnd(raio_pipeline_t* pipe, raio_type_t next_step) {
    return 0;
}

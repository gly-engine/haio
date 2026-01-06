#pragma once
#include "raio.h"

const raio_worker_t raio_codec_workers[] = {
    {
        .steps = { RAIO_TYPE_BUFFER, RAIO_TYPE_BUFFER_FULL },
        .func = BufferAggregatorUntilZero
    },
    {
        .steps = { RAIO_TYPE_BUFFER_FULL, RAIO_TYPE_IMG_PNG, RAIO_TYPE_IMG_RGBA8 },
        .func = DecodePngBufferToRGBA8
    },
    {
        .steps = { RAIO_TYPE_IMG_RGBA8, RAIO_TYPE_IMG_PPM },
        .func = EncodeRGBA8ToPPM
    },
    {
        .steps = { RAIO_TYPE_IMG_RGBA8, RAIO_TYPE_IMG_BMP },
        .func = ConvertNotImplemented
    },
    {
        .steps = { RAIO_TYPE_IMG_RGBA8, RAIO_TYPE_IMG_YUV420 },
        .func = ConvertRGBA8ToYUV420
    },
    {
        .steps = { RAIO_TYPE_IMG_YUV420, RAIO_TYPE_IMG_Y4M420 },
        .func = EncodeYUV420ToY4M
    },
    {
        .steps = { RAIO_TYPE_BUFFER_GZIP },
        .func = ConvertNotImplemented
    }
};

const size_t raio_codec_workers_len = sizeof(raio_codec_workers)/sizeof(raio_worker_t);

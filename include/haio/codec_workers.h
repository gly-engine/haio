#pragma once
#include "haio.h"

const haio_worker_t haio_codec_workers[] = {
    {
        .steps = { HAIO_TYPE_BUFFER, HAIO_TYPE_BUFFER_FULL },
        .func = BufferAggregatorUntilZero
    },
    {
        .steps = { HAIO_TYPE_BUFFER_FULL, HAIO_TYPE_IMG_PNG, HAIO_TYPE_IMG_RGBA8 },
        .func = DecodePngBufferToRGBA8
    },
    {
        .steps = { HAIO_TYPE_IMG_RGBA8, HAIO_TYPE_IMG_PPM },
        .func = EncodeRGBA8ToPPM
    },
    {
        .steps = { HAIO_TYPE_IMG_RGBA8, HAIO_TYPE_IMG_BMP },
        .func = ConvertNotImplemented
    },
    {
        .steps = { HAIO_TYPE_IMG_RGBA8, HAIO_TYPE_IMG_YUV420 },
        .func = ConvertRGBA8ToYUV420
    },
    {
        .steps = { HAIO_TYPE_IMG_YUV420, HAIO_TYPE_IMG_Y4M420 },
        .func = EncodeYUV420ToY4M
    },
    {
        .steps = { HAIO_TYPE_BUFFER_GZIP },
        .func = ConvertNotImplemented
    }
};

const size_t haio_codec_workers_len = sizeof(haio_codec_workers)/sizeof(haio_worker_t);

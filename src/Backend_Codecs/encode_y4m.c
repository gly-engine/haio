#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixfmt.h>

#include "raio.h"

void EncodeYUV420ToY4M(raio_worker_handle_t *handle, raio_buffer_t *src, raio_buffer_t *dst) {
    /*if (!bmp || !out_size || bmp->buffer_len == 0) return NULL;

    int w = bmp->width;
    int h = bmp->height;

    char header[128];
    int header_len = snprintf(
        header, sizeof(header),
        "YUV4MPEG2 W%d H%d F1:1 Ip A1:1 C420\nFRAME\n",
        w, h
    );

    struct SwsContext *sws = sws_getContext(
        w, h, AV_PIX_FMT_RGBA,
        w, h, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR,
        NULL, NULL, NULL
    );
    if (!sws) return NULL;

    sws_setColorspaceDetails(
        sws,
        sws_getCoefficients(SWS_CS_DEFAULT),
        0,
        sws_getCoefficients(SWS_CS_DEFAULT),
        0,
        0,
        1 << 16,
        1 << 16
    );

    uint8_t *src_data[4] = { bmp->pixels, NULL, NULL, NULL };
    int src_linesize[4] = { w * 4, 0, 0, 0 };

    uint8_t *yuv_data[4];
    int yuv_linesize[4];

    int yuv_size = av_image_alloc(
        yuv_data, yuv_linesize,
        w, h,
        AV_PIX_FMT_YUV420P,
        1
    );
    if (yuv_size < 0) {
        sws_freeContext(sws);
        return NULL;
    }

    sws_scale(
        sws,
        (const uint8_t * const *)src_data,
        src_linesize,
        0,
        h,
        yuv_data,
        yuv_linesize
    );

    size_t total_size = header_len + yuv_size;
    uint8_t *out = malloc(total_size);
    if (!out) {
        av_freep(&yuv_data[0]);
        sws_freeContext(sws);
        return NULL;
    }

    memcpy(out, header, header_len);
    memcpy(out + header_len, yuv_data[0], yuv_size);

    *out_size = total_size;

    av_freep(&yuv_data[0]);
    sws_freeContext(sws);

    return out;
    */
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "img.h"
/*

uint8_t *EncodeInternalToPPM(bitmap_t *bmp, size_t *out_size) {
    if (!bmp || bmp->buffer_len == 0 || !out_size) return NULL;

    char header[64];
    int header_len = snprintf(header, sizeof(header), "P6\n%d %d\n255\n", bmp->width, bmp->height);

    size_t data_size = bmp->width * bmp->height * 3;
    size_t total_size = header_len + data_size;

    uint8_t *buffer = malloc(total_size);
    if (!buffer) return NULL;

    memcpy(buffer, header, header_len);

    uint8_t *dst = buffer + header_len;
    for (int y = 0; y < bmp->height; y++) {
        for (int x = 0; x < bmp->width; x++) {
            uint8_t *p = &bmp->pixels[(y * bmp->width + x) * 4]; // RGBA
            *dst++ = p[0]; // R
            *dst++ = p[1]; // G
            *dst++ = p[2]; // B
        }
    }

    *out_size = total_size;
    return buffer;
}
*/
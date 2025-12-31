#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "img.h"

void EncodeRGBAToPPMBuffer(raio_worker_handle_t *handle, raio_buffer_t *src, raio_buffer_t *dst) {
    do {
        if (handle->done) break;
        if (!src || !dst || !handle) assert(false);

        if (!dst->data.u8) {
            char header[64];
            int header_len = snprintf(header, sizeof(header), "P6\n%d %d\n255\n", handle->width, handle->height);

            size_t row_size = handle->width * 3; // RGB
            size_t initial_capacity = header_len + row_size;

            dst->data.u8 = malloc(initial_capacity);
            if (!dst->data.u8) assert(false);
            dst->capacity = initial_capacity;
            dst->len = 0;
            dst->pos = 0;

            memcpy(dst->data.u8, header, header_len);
            dst->pos = header_len;

            handle->buffer_aux.len = row_size;
            handle->buffer_aux.capacity = row_size;
            handle->buffer_aux.data.u8 = malloc(row_size);
            if (!handle->buffer_aux.data.u8) assert(false);

            handle->lines = 0;
        }

        if (handle->lines < handle->height) {
            size_t row_size = handle->width * 3;
            uint8_t *line_src = src->data.u8 + handle->lines * handle->width * 4;
            uint8_t *line_dst = handle->buffer_aux.data.u8;

            for (int x = 0; x < handle->width; x++) {
                line_dst[x*3 + 0] = line_src[x*4 + 0]; // R
                line_dst[x*3 + 1] = line_src[x*4 + 1]; // G
                line_dst[x*3 + 2] = line_src[x*4 + 2]; // B
            }

            if (dst->pos + row_size > dst->capacity) {
                size_t new_capacity = dst->capacity * 2;
                while (dst->pos + row_size > new_capacity) new_capacity *= 2;
                uint8_t *new_buf = realloc(dst->data.u8, new_capacity);
                if (!new_buf) assert(false);
                dst->data.u8 = new_buf;
                dst->capacity = new_capacity;
            }

            memcpy(dst->data.u8 + dst->pos, line_dst, row_size);
            dst->pos += row_size;
            handle->lines++;

            break;
        }

        handle->done = true;
        free(handle->buffer_aux.data.u8);

    } while(0);
}

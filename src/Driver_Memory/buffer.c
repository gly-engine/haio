#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "img.h"

#define INITIAL_SIZE 512

int BufferReset(raio_buffer_t *buf) {
    buf->len = 0;
    return 0;   
}

int BufferEnsureCapacity(raio_buffer_t *buf, size_t len) {
    if (buf->len + len > buf->size) {
        size_t new_size = buf->size ? buf->size * 2 : INITIAL_SIZE;

        while (new_size < buf->len + len)
            new_size *= 2;

        uint8_t *new_data = realloc(buf->data.ptr, new_size);
        if (!new_data) {
            free(buf->data.ptr);
            buf->data.ptr = NULL;
            buf->len = 0;
            buf->size = 0;
            return 1;
        }

        buf->data.ptr = new_data;
        buf->size = new_size;
    }
    return 0;
}

int BufferPush(raio_buffer_t *buf, const void *data, size_t len) {
    if (!buf) return 1;
    if (!BufferEnsureCapacity(buf, len)) return 1;

    memcpy(buf->data.ptr + buf->len, data, len);
    buf->len += len;
    return 0;
}

/**
 * @details move the buffer from @c src to @c dst ,
 * preserving the largest allocation, and clears src.
 *
 * @param [in,out] src
 * @param [in,out] dst
 */
int BufferMove(raio_buffer_t *src, raio_buffer_t *dst) {
    if (src->size > dst->size) {
        free(dst->data.ptr);
        dst->data.ptr = src->data.ptr;
        dst->size = src->size;
    } else {
        memcpy(dst->data.ptr, src->data.ptr, src->len);
        free(src->data.ptr);
    }

    dst->len = src->len;

    src->data.ptr = NULL;
    src->size = 0;
    src->len = 0;
    return 0;
}
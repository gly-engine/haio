#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "img.h"

#define INITIAL_SIZE 512

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

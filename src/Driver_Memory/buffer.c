#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>

#include "haio.h"

#define INITIAL_SIZE 512

void BufferReset(haio_buffer_t *buf) {
    buf->len = 0;  
    buf->pos = 0; 
}

int BufferEnsureCapacity(haio_buffer_t *buf, size_t len) {
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

int BufferPush(haio_buffer_t *buf, const void *data, size_t len) {
    if (!buf) return 1;
    if (BufferEnsureCapacity(buf, len + buf->len)) return 2;

    memcpy(buf->data.u8 + buf->len, data, len);
    buf->len += len;
    return 0;
}

/**
 * @brief Appends packed `4 -> 3` data to the buffer.
 *
 * @details Converts interleaved 4-byte elements into packed 3-byte elements
 * by skipping the 4th byte (alpha channel). Commonly used to copy
 * data from @c RGBA to @c RGB.
 *
 * @param[out] buf
 * @param[in] data
 * @param[in] len
 *
 * @retval 0 Success.
 */
int BufferPush43(haio_buffer_t *buf, const void *data, size_t len) {
    if (!buf || !data) return 1;

    size_t pixels = len / 4;
    size_t out_len = pixels * 3;

    if (pixels == 0) return 3;

    size_t needed  = buf->len + out_len;
    size_t rounded = (needed + 11) / 12 * 12;

    if(BufferEnsureCapacity(buf, rounded)) return 2;

    uint8_t *dst_ptr = &buf->data.u8[buf->len];
    uint8_t *dst_end = &dst_ptr[out_len];
    const uint8_t *src_ptr = (const uint8_t *)data;

    while (dst_ptr < dst_end) {
        *dst_ptr++ = *src_ptr++;
        *dst_ptr++ = *src_ptr++;
        *dst_ptr++ = *src_ptr++;
        src_ptr++; 
    }

    buf->len += out_len;
    return 0;
}

int BufferPushString(haio_buffer_t *buf, const char *fmt, ...)
{
    if (!buf || !fmt) return 1;

    va_list args;
    va_start(args, fmt);

    va_list args_copy;
    va_copy(args_copy, args);

    int needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (needed < 0) {
        va_end(args_copy);
        return 2;
    }

    if(BufferEnsureCapacity(buf, buf->len + (size_t)needed + 1)) return 2;

    vsnprintf(&buf->data.str[buf->len], buf->size, fmt, args_copy);
    va_end(args_copy);

    buf->len += (size_t)needed;
    return 0;
}

size_t BufferPop(haio_buffer_t *buf, void *out, size_t len) {
    if (!buf || !out) return 0;

    size_t available = buf->len - buf->pos;
    if (available == 0) return 0;

    size_t to_copy = (len < available) ? len : available;

    memcpy(out, buf->data.u8 + buf->pos, to_copy);
    buf->pos += to_copy;

    if (buf->pos >= buf->len) {
        buf->len = 0;
        buf->pos = 0;
    }

    return to_copy;
}

bool BufferHasQeue(haio_buffer_t *buf) {
    return buf && buf->len > 0 && buf->pos > 0 && buf->pos < buf->len;
}

/**
 * @details move the buffer from @c src to @c dst ,
 * preserving the largest allocation, and clears src.
 *
 * @param [in,out] src
 * @param [in,out] dst
 */
int BufferMove(haio_buffer_t *src, haio_buffer_t *dst) {
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
    src->pos = 0;
    return 0;
}

void BufferClose(haio_buffer_t *buf) {
    if (!buf) return;
    if (!buf->data.ptr || !buf->len) return;
    free(buf->data.ptr);
    buf->data.ptr = NULL;
    buf->size = 0;
    buf->len = 0;
    buf->pos = 0;
}

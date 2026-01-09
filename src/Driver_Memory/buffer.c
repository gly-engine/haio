#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <immintrin.h>

#include "haio.h"

#define INITIAL_SIZE 512

static inline void BufferPush43_scalar(uint8_t *dst, const uint8_t *src, size_t pixels) {
    while (pixels--) {
        *dst++ = *src++;
        *dst++ = *src++;
        *dst++ = *src++;
        src++;
    }
}

__attribute__((target("avx2")))
static inline void BufferPush43_avx2(uint8_t *dst, const uint8_t *src, size_t pixels) {
    size_t simd_pixels = pixels & ~7ULL;

    const __m256i shuffle = _mm256_setr_epi8(
         0,  1,  2,   4,  5,  6,   8,  9,
        10, 12, 13, 14, 16, 17, 18, 20,
        21, 22, 24, 25, 26, 28, 29, 30,
        -1, -1, -1, -1, -1, -1, -1, -1
    );

    size_t i = 0;
    for (; i < simd_pixels; i += 8) {
        __m256i v = _mm256_loadu_si256((const __m256i *)src);
        __m256i r = _mm256_shuffle_epi8(v, shuffle);

        _mm_storeu_si128((__m128i *)dst,
                         _mm256_castsi256_si128(r));
        _mm_storel_epi64((__m128i *)(dst + 16),
                         _mm256_extracti128_si256(r, 1));

        src += 32;
        dst += 24;
    }

    BufferPush43_scalar(dst, src, pixels - simd_pixels);
}

__attribute__((target("ssse3")))
static inline void BufferPush43_ssse3(uint8_t *dst, const uint8_t *src, size_t pixels) {
    size_t simd_pixels = pixels & ~3ULL;

    const __m128i shuffle = _mm_setr_epi8(
         0,  1,  2,   4,  5,  6,
         8,  9, 10,  12, 13, 14,
        -1, -1, -1, -1
    );

    size_t i = 0;
    for (; i < simd_pixels; i += 4) {
        __m128i v = _mm_loadu_si128((const __m128i *)src);
        __m128i r = _mm_shuffle_epi8(v, shuffle);

        _mm_storeu_si128((__m128i *)dst, r);

        src += 16;
        dst += 12;
    }

    BufferPush43_scalar(dst, src, pixels - simd_pixels);
}

int BufferEnsureCapacity(haio_buffer_t *buf, size_t len)
{
    size_t required = buf->len + len;

    if (required > buf->size) {
        size_t new_size = buf->size ? buf->size * 2 : INITIAL_SIZE;

        while (new_size < required)
            new_size *= 2;


        new_size = (new_size + 32) & ~(size_t)32;

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
 * data from @c RGBA to @c RGB
 *
 * @param[out] buf
 * @param[in] data
 * @param[in] len
 *
 * @retval 0 Success.
 */
int BufferPush43(haio_buffer_t *buf, const void *data, size_t len)
{
    if (!buf || !data) return 1;

    size_t pixels  = len >> 2;
    if (pixels == 0) return 3;

    size_t out_len = pixels * 3;
    size_t needed  = buf->len + out_len;

    if (BufferEnsureCapacity(buf, needed)) return 2;

    uint8_t *dst = buf->data.u8 + buf->len;
    const uint8_t *src = (const uint8_t *)data;

    if (__builtin_cpu_supports("avx2")) {
        BufferPush43_avx2(dst, src, pixels);
    }
    else if (__builtin_cpu_supports("ssse3")) {
        BufferPush43_ssse3(dst, src, pixels);
    }
    else {
        BufferPush43_scalar(dst, src, pixels);
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

void BufferReset(haio_buffer_t *buf) {
    buf->len = 0;  
    buf->pos = 0; 
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

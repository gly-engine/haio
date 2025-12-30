#include "img.h"
#include <stdint.h>
#include <stddef.h>

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define SIZE(n) (0xFFFFFFFF<<((4-n)*8))
#elif defined(__BYTE_ORDER__)
#define SIZE(n) (0xFFFFFFFF>>((4-n)*8))
#else
#error undefined __BYTE_ORDER__
#endif

typedef struct {
    union {
        char     str[4];   /* zero-padded */
        uint32_t u32;
    } ext;
    uint32_t     mask;
    raio_type_t format;
} raio_ext_t;

/**
 * @pre must be ordered by @c.ext.str_
 */
static const raio_ext_t extensions[] = {
    { "png", SIZE(3), RAIO_TYPE_IMG_PNG },
    { "ppm", SIZE(3), RAIO_TYPE_IMG_PPM },
    { "y4m", SIZE(3), RAIO_TYPE_IMG_Y4M_420 }
};

raio_type_t GetFormatFromExtension(char *const txt)
{
    if (!txt || !txt[0]) {
        return RAIO_TYPE_NULL;
    }

    uint32_t key;
    __builtin_memcpy(&key, txt, sizeof(key));

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    key = __builtin_bswap32(key);
#endif

    key |= 0x20202020u;

    size_t lo = 0;
    size_t hi = sizeof(extensions) / sizeof(extensions[0]);

    while (lo < hi) {
        size_t mid = lo + ((hi - lo) >> 1);
        uint32_t k = extensions[mid].mask & key;

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
        uint32_t ext = __builtin_bswap32(extensions[mid].ext.u32);
#else 
        uint32_t ext = extensions[mid].ext.u32;
#endif

        if (k < ext) {
            hi = mid;
        } else if (k > ext) {
            lo = mid + 1;
        } else {
            return extensions[mid].format;
        }
    }
    return RAIO_TYPE_NULL;
}

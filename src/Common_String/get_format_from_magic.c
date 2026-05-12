#include <stddef.h>
#include <string.h>

#include "haio.h"

haio_type_t GetFormatFromMagic(const unsigned char *buffer, size_t len)
{
    static const unsigned char png_signature[] = {
        0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'
    };

    if (len >= sizeof(png_signature) &&
        memcmp(buffer, png_signature, sizeof(png_signature)) == 0) {
        return HAIO_TYPE_IMG_PNG;
    }

    return HAIO_TYPE_NULL;
}

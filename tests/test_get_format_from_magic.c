#include <stddef.h>
#include <assert.h>

#include "haio.h"
#include "haio/functions.h"

int main()
{
    static const unsigned char png[] = {
        0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'
    };
    static const unsigned char ppm[] = { 'P', '6', '\n' };

    assert(GetFormatFromMagic(png, sizeof(png)) == HAIO_TYPE_IMG_PNG);
    assert(GetFormatFromMagic(ppm, sizeof(ppm)) == HAIO_TYPE_NULL);
    assert(GetFormatFromMagic(NULL, 0) == HAIO_TYPE_NULL);

    return 0;
}

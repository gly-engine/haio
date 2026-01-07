#include <stddef.h>
#include <assert.h>

#include "raio.h"
#include "raio/functions.h"

int main()
{
    // safe checks
    assert(GetFormatFromExtension("") == RAIO_TYPE_NULL);
    assert(GetFormatFromExtension(NULL) == RAIO_TYPE_NULL);
    assert(GetFormatFromExtension("nil") == RAIO_TYPE_NULL);
    // correct extensions
    assert(GetFormatFromExtension("png")== RAIO_TYPE_IMG_PNG);
    assert(GetFormatFromExtension("ppm")== RAIO_TYPE_IMG_PPM);
    assert(GetFormatFromExtension("y4m")== RAIO_TYPE_IMG_Y4M420);
    // case unsensitive
    assert(GetFormatFromExtension("Png") == RAIO_TYPE_IMG_PNG);
    assert(GetFormatFromExtension("PNG") == RAIO_TYPE_IMG_PNG);
    assert(GetFormatFromExtension("pNg") == RAIO_TYPE_IMG_PNG);
    // protection: memory unaligned
    static const volatile char png[] = " png";
    static const volatile char ppm[] = "  ppm";
    static const volatile char y4m[] = "   y4m";
    assert(GetFormatFromExtension((char *const) &png[1]) == RAIO_TYPE_IMG_PNG);
    assert(GetFormatFromExtension((char *const) &ppm[2]) == RAIO_TYPE_IMG_PPM);
    assert(GetFormatFromExtension((char *const) &y4m[3]) == RAIO_TYPE_IMG_Y4M420);
    // protection: memory invasion
    static const char gz[] = {'g', 'z'};
    assert(GetFormatFromExtension((char *const) gz) == RAIO_TYPE_NULL);
    return 0;
}

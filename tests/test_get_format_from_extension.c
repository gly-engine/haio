#include <stddef.h>
#include <assert.h>

#include "haio.h"
#include "haio/functions.h"

int main()
{
    // safe checks
    assert(GetFormatFromExtension("") == HAIO_TYPE_NULL);
    assert(GetFormatFromExtension(NULL) == HAIO_TYPE_NULL);
    assert(GetFormatFromExtension("nil") == HAIO_TYPE_NULL);
    // correct extensions
    assert(GetFormatFromExtension("png")== HAIO_TYPE_IMG_PNG);
    assert(GetFormatFromExtension("ppm")== HAIO_TYPE_IMG_PPM);
    assert(GetFormatFromExtension("y4m")== HAIO_TYPE_IMG_Y4M420);
    // case unsensitive
    assert(GetFormatFromExtension("Png") == HAIO_TYPE_IMG_PNG);
    assert(GetFormatFromExtension("PNG") == HAIO_TYPE_IMG_PNG);
    assert(GetFormatFromExtension("pNg") == HAIO_TYPE_IMG_PNG);
    // protection: memory unaligned
    static const volatile char png[] = " png";
    static const volatile char ppm[] = "  ppm";
    static const volatile char y4m[] = "   y4m";
    assert(GetFormatFromExtension((char *const) &png[1]) == HAIO_TYPE_IMG_PNG);
    assert(GetFormatFromExtension((char *const) &ppm[2]) == HAIO_TYPE_IMG_PPM);
    assert(GetFormatFromExtension((char *const) &y4m[3]) == HAIO_TYPE_IMG_Y4M420);
    // protection: memory invasion
    static const char gz[] = {'g', 'z'};
    assert(GetFormatFromExtension((char *const) gz) == HAIO_TYPE_NULL);
    return 0;
}

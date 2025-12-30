#include <stdint.h>
#include <stddef.h>

typedef struct {
    int16_t width;
    int16_t height;
    size_t buffer_len;
    uint8_t pixels[];    
} bitmap_t;


int FrontendConvertCli(int argc, char* argv[]);
// Backends:
bitmap_t *DecodePngBufferToInternal(const void *png_data, size_t png_size);
uint8_t *EncodeInternalToPPM(bitmap_t *bmp, size_t *out_size);
uint8_t *EncodeInternalToY4M(bitmap_t *bmp, size_t *out_size);

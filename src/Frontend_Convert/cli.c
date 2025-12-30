#include <stdio.h>
#include <stdlib.h>

#include "img.h"

int FrontendConvertCli(int argc, char* argv[]) {
    if (argc <= 2) {
        printf("usage:\n./img convert input.png output.png\n");
        return 1;
    }

    const char *input_file = argv[1];
    const char *output_file = argv[2];

    FILE *fin = NULL;
    FILE *fout = NULL;
    uint8_t *png_buffer = NULL;
    bitmap_t *bmp = NULL;
    uint8_t *ppm_buffer = NULL;
    size_t png_size = 0, ppm_size = 0;

    do {
        fin = fopen(input_file, "rb");
        if (!fin) {
            fprintf(stderr, "Failed to open %s\n", input_file);
            break;
        }

        fseek(fin, 0, SEEK_END);
        png_size = ftell(fin);
        fseek(fin, 0, SEEK_SET);

        png_buffer = malloc(png_size);
        if (!png_buffer) {
            fprintf(stderr, "Memory allocation failed\n");
            break;
        }

        if (fread(png_buffer, 1, png_size, fin) != png_size) {
            fprintf(stderr, "Failed to read %s\n", input_file);
            break;
        }

        bmp = DecodePngBufferToInternal(png_buffer, png_size);
        if (!bmp) {
            fprintf(stderr, "Failed to decode PNG\n");
            break;
        }

        ppm_buffer = EncodeInternalToPPM(bmp, &ppm_size);
        if (!ppm_buffer) {
            fprintf(stderr, "Failed to generate PPM\n");
            break;
        }

        fout = fopen(output_file, "wb");
        if (!fout) {
            fprintf(stderr, "Failed to create %s\n", output_file);
            break;
        }

        if (fwrite(ppm_buffer, 1, ppm_size, fout) != ppm_size) {
            fprintf(stderr, "Failed to write %s\n", output_file);
            break;
        }

        printf("File successfully converted: %s\n", output_file);
    } while (0);

    if (fin) fclose(fin);
    if (fout) fclose(fout);
    if (png_buffer) free(png_buffer);
    if (ppm_buffer) free(ppm_buffer);
    if (bmp) {
        free(bmp);
    }
    
    return 0;
}

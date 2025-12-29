#include <png.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    FILE *fp = fopen("one_pixel.png", "wb");
    if (!fp) {
        perror("fopen");
        return 1;
    }

    png_structp png_ptr =
        png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fclose(fp);
        return 1;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, NULL);
        fclose(fp);
        return 1;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        return 1;
    }

    png_init_io(png_ptr, fp);

    // PNG 1x1 RGBA 8-bit
    png_set_IHDR(
        png_ptr,
        info_ptr,
        1, 1,                 // width, height
        8,                    // bit depth
        PNG_COLOR_TYPE_RGBA,  // color type
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
    );

    png_write_info(png_ptr, info_ptr);

    // Pixel RGBA (vermelho opaco)
    png_byte pixel[4] = {255, 0, 0, 255};

    png_bytep row_pointers[1];
    row_pointers[0] = pixel;

    png_write_image(png_ptr, row_pointers);
    png_write_end(png_ptr, NULL);

    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);

    return 0;
}

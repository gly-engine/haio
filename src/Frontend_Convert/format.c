#include <stdio.h>
#include <string.h>

#include "haio.h"
#include "haio/functions.h"
#include "convert.h"

static char ascii_lower(char c)
{
    if ('A' <= c && c <= 'Z') {
        return (char)(c + ('a' - 'A'));
    }

    return c;
}

bool convert_slice_iequals(char *txt, size_t len, const char *expected)
{
    if (strlen(expected) != len) {
        return false;
    }

    for (size_t i = 0; i < len; i++) {
        if (ascii_lower(txt[i]) != ascii_lower(expected[i])) {
            return false;
        }
    }

    return true;
}

char *convert_find_format_separator(char *txt)
{
    char *p = txt;

    while (*p) {
        if (*p == '/' || *p == '\\') {
            return NULL;
        }

        if (*p == ':') {
            return p == txt ? NULL : p;
        }

        p++;
    }

    return NULL;
}


bool convert_known_format_prefix(char *name, size_t len)
{
    return convert_slice_iequals(name, len, "png") ||
        convert_slice_iequals(name, len, "ppm") ||
        convert_slice_iequals(name, len, "y4m") ||
        convert_slice_iequals(name, len, "jpg") ||
        convert_slice_iequals(name, len, "jpeg");
}

haio_type_t convert_format_from_name(char *name, size_t len)
{
    if (!name || !len) {
        return HAIO_TYPE_NULL;
    }

    if (convert_slice_iequals(name, len, "png")) {
        return HAIO_TYPE_IMG_PNG;
    }

    if (convert_slice_iequals(name, len, "ppm")) {
        return HAIO_TYPE_IMG_PPM;
    }

    if (convert_slice_iequals(name, len, "y4m")) {
        return HAIO_TYPE_IMG_Y4M420;
    }

    return HAIO_TYPE_NULL;
}

haio_type_t convert_format_from_path(char *path)
{
    char *ext = GetExtensionFromString(path);

    if (!ext) {
        return HAIO_TYPE_NULL;
    }

    return GetFormatFromExtension(ext);
}

haio_type_t convert_input_format_from_bytes(const unsigned char *buffer, size_t len)
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

haio_type_t convert_input_format_from_file(FILE *f_in, char *path)
{
    unsigned char buffer[CONVERT_FORMAT_PROBE_SIZE];
    haio_type_t format;
    size_t nread;

    nread = fread(buffer, sizeof(unsigned char), sizeof(buffer), f_in);
    format = convert_input_format_from_bytes(buffer, nread);

    if (fseek(f_in, 0, SEEK_SET)) {
        return HAIO_TYPE_NULL;
    }

    if (format != HAIO_TYPE_NULL) {
        return format;
    }

    return convert_format_from_path(path);
}

void convert_print_unsupported_format(const char *kind, char *format_name, char *fallback)
{
    char *sep = format_name ? convert_find_format_separator(format_name) : NULL;

    if (sep) {
        printf("[error] unsupported %s format: ", kind);
        fwrite(format_name, sizeof(char), (size_t)(sep - format_name), stdout);
        printf("\n");
        return;
    }

    printf("[error] unknown %s format: %s\n", kind, fallback);
}

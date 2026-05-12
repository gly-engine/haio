#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "haio.h"
#include "haio/functions.h"
#include "convert.h"

bool convert_slice_iequals(char *txt, size_t len, const char *expected)
{
    if (strlen(expected) != len) {
        return false;
    }

    for (size_t i = 0; i < len; i++) {
        if (tolower((unsigned char)txt[i]) != tolower((unsigned char)expected[i])) {
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
    return convert_format_from_name(name, len) != HAIO_TYPE_NULL;
}

haio_type_t convert_format_from_name(char *name, size_t len)
{
    char ext[4] = { 0 };

    if (!name || !len) {
        return HAIO_TYPE_NULL;
    }

    if (len > sizeof(ext)) {
        return HAIO_TYPE_NULL;
    }

    memcpy(ext, name, len);
    return GetFormatFromExtension(ext);
}

haio_type_t convert_format_from_path(char *path)
{
    char *ext = GetExtensionFromString(path);

    if (!ext) {
        return HAIO_TYPE_NULL;
    }

    return GetFormatFromExtension(ext);
}

haio_type_t convert_input_format_from_file(FILE *f_in, char *path)
{
    unsigned char buffer[CONVERT_FORMAT_PROBE_SIZE];
    haio_type_t format;
    size_t nread;

    nread = fread(buffer, sizeof(unsigned char), sizeof(buffer), f_in);
    format = GetFormatFromMagic(buffer, nread);

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

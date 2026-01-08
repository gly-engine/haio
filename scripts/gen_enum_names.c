#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

int main() {
    FILE *f = fopen(HAIO_HEADER, "r");
    assert(f);

    char buf[1024];
    int count = 0;

    printf("const char *const haio_types_names[] = {");

    while (fgets(buf, sizeof(buf), f)) {
        char *p = buf;
        while ((p = strstr(p, "HAIO_TYPE_"))) {
            int len = 0;
            while (isalnum(p[len]) || p[len] == '_')
                len++;

            if (!strncmp(p, "HAIO_TYPE_NULL", len) || !strncmp(p, "HAIO_TYPE_COUNT", len)) {
                p += len;
                continue;
            }

            printf("%s\"%.*s\"", count++? ",\n  ": "\n  ", len, p);
            p += len;
        }
    }

    printf("\n};\n\nconst unsigned int haio_types_names_len = %d;\n", count);
    fclose(f);

    return 0;
}

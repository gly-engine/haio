#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

int main() {
    FILE *f = fopen(RAIO_HEADER, "r");
    assert(f);

    char buf[1024];
    int first = 1;

    printf("static const char *const raio_types_names[] = {");

    while (fgets(buf, sizeof(buf), f)) {
        char *p = buf;
        while ((p = strstr(p, "RAIO_TYPE_"))) {
            int len = 0;
            while (isalnum(p[len]) || p[len] == '_')
                len++;

            printf("%s\"%.*s\"", first? "\n  ": ",\n  ", len, p);
            first = 0;

            p += len;
        }
    }

    printf("\n};\n");
    fclose(f);

    return 0;
}

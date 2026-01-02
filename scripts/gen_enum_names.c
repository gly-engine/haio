#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

int main() {
    FILE *f = fopen(RAIO_HEADER, "r");
    assert(f);

    char buf[1024];
    int count = 0;

    printf("const char *const raio_types_names[] = {");

    while (fgets(buf, sizeof(buf), f)) {
        char *p = buf;
        while ((p = strstr(p, "RAIO_TYPE_"))) {
            int len = 0;
            while (isalnum(p[len]) || p[len] == '_')
                len++;

            printf("%s\"%.*s\"", count++? ",\n  ": "\n  ", len, p);
            p += len;
        }
    }

    printf("\n};\n\nconst unsigned int raio_types_names_len = %d;\n", count);
    fclose(f);

    return 0;
}

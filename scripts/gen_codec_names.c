#include <stdio.h>
#include <string.h>
#include <ctype.h>

void process(char* src, int len) {
    printf(" -> %.*s\n", len, src);
}

int main() {
    static const char str[] = RAIO_CODECS;
    char *p = str;
    char *end;

    while (*p != '\0') {
        end = strchr(p, ',');
        if (end == NULL) {
            process(p, strlen(p));
            break;
        } else {
            process(p, end - p);
            p = end + 1;
        }
    }

    return 0;
}

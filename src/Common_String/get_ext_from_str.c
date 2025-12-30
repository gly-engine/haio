#include <stddef.h>

char* GetExtensionFromString(char *const txt)
{
    if (!txt) return NULL;

    char *last_dot = NULL;
    char *p = txt;

    while (*p != '\0') {
        if (*p == '.') {
            last_dot = p;
        }
        p++;
    }

    if (!last_dot || last_dot == txt) {
        return NULL;
    }

    return last_dot + 1;
}

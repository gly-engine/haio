#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define LINE_SIZE 1024
#define MAX_FUNCS 1024

typedef struct {
    char *type;
    char *name;
    char *params;
} FuncInfo;

FuncInfo functions[MAX_FUNCS];
int func_count = 0;

int cmp_func(const void *a, const void *b) {
    const FuncInfo *fa = (const FuncInfo *)a;
    const FuncInfo *fb = (const FuncInfo *)b;
    return strcmp(fa->name, fb->name);
}

static int is_ident_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

void add_function(const char *line) {
    if (!line || !*line)
        return;

    if (isspace((unsigned char)line[0]))
        return;

    if (line[0] == '#')
        return;

    const char *lparen = strchr(line, '(');
    if (!lparen)
        return;

    const char *p = lparen;
    while (p > line && isspace((unsigned char)p[-1]))
        p--;

    const char *name_end = p;
    while (p > line && is_ident_char(p[-1]))
        p--;

    const char *name_start = p;

    if (name_start == name_end)
        return;

    if (!isupper((unsigned char)name_start[0]))
        return;

    for (const char *t = line; t < name_start; t++) {
        if (!strncmp(t, "static", 6) &&
            (t == line || !is_ident_char(t[-1])) &&
            !is_ident_char(t[6])) {
            return;
        }
    }

    const char *rparen = strchr(lparen, ')');
    if (!rparen)
        return;

    functions[func_count].type   = strndup(line, name_start - line);
    functions[func_count].name   = strndup(name_start, name_end - name_start);
    functions[func_count].params = strndup(lparen + 1, rparen - lparen - 1);

    func_count++;
}


void print_functions() {
    int first = 1;

#ifdef RAIO_STUB
    printf("#ifdef RAIO_STUB\n");
    for (int i = 0; i < func_count; i++) {
        printf("%s %s(%s) {}\n", functions[i].type, functions[i].name, functions[i].params);
    }
    printf("#else\n");
#endif
    for (int i = 0; i < func_count; i++) {
        printf("%s %s(%s);\n", functions[i].type, functions[i].name, functions[i].params);
    }
#ifdef RAIO_STUB
    printf("#endif\n\n");
#endif

#ifdef RAIO_NAMES
    printf("#ifndef RAIO_ONLY_PROTO\nconst struct {\n  raio_worker_func_t func;\n  const char* name;\n} raio_codec_names[] = {");
    for (int i = 0; i < func_count; i++) {
        printf("%s{%s, \"%s\"}", first? "\n  ": ",\n  ", functions[i].name, functions[i].name);
        first = 0;
    }
    printf("\n};\n\nconst unsigned int raio_codec_names_len = %d;\n#endif\n", func_count);
#endif
}

void free_functions() {
    for (int i = 0; i < func_count; i++) {
        free(functions[i].type);
        free(functions[i].name);
        free(functions[i].params);
    }
}

void load_functions() {
    static const char str[] = RAIO_INPUT;
    char *p = (char *)str;
    char *end;

    while (*p) {
        end = strchr(p, ',');
        int len = end ? (end - p) : strlen(p);

        char filename[256];
        strncpy(filename, p, len);
        filename[len] = '\0';

        FILE *f = fopen(filename, "r");
        if (f) {
            char line[LINE_SIZE];
            while (fgets(line, LINE_SIZE, f)) {
                add_function(line);
            }
            fclose(f);
        }

        if (!end) break;
        p = end + 1;
    }
}

int main() {
    load_functions();
    qsort(functions, func_count, sizeof(FuncInfo), cmp_func);
    print_functions();
    free_functions();
    return 0;
}

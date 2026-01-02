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

void add_function(const char *line) {
    const char *p = line;
    while (*p && isspace(*p)) p++;
    if (strncmp(p, "void ", 5) != 0) return;

    p += 5;
    const char *name_end = strchr(p, '(');
    if (!name_end) return;
    const char *params_end = strchr(name_end, ')');
    if (!params_end) return;

    int name_len = name_end - p;
    int params_len = params_end - name_end - 1;

    functions[func_count].type = strdup("void");
    functions[func_count].name = malloc(name_len + 1);
    strncpy(functions[func_count].name, p, name_len);
    functions[func_count].name[name_len] = '\0';

    functions[func_count].params = malloc(params_len + 1);
    strncpy(functions[func_count].params, name_end + 1, params_len);
    functions[func_count].params[params_len] = '\0';

    func_count++;
}

void print_functions() {
    int first = 1;

    printf("#ifdef RAIO_STUB\n");
    for (int i = 0; i < func_count; i++) {
        printf("%s %s(%s) {}\n", functions[i].type, functions[i].name, functions[i].params);
    }
    printf("#else\n");
    for (int i = 0; i < func_count; i++) {
        printf("%s %s(%s);\n", functions[i].type, functions[i].name, functions[i].params);
    }
    printf("#endif\n\nconst struct {\n  void (*func)(void);\n  const char* nome;\n} raio_codec_names[] = {");
    for (int i = 0; i < func_count; i++) {
        printf("%s{%s, \"%s\"}", first? "\n  ": ",\n  ", functions[i].name, functions[i].name);
        first = 0;
    }
    printf("\n};\n\nconst unsigned int raio_codec_names_len = %d;\n", func_count);
}

void free_functions() {
    for (int i = 0; i < func_count; i++) {
        free(functions[i].type);
        free(functions[i].name);
        free(functions[i].params);
    }
}

void load_functions() {
    static const char str[] = RAIO_CODECS;
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

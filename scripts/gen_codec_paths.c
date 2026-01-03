#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#define RAIO_STUB

#include "raio.h"

#include "raio/enum_names.h"
#include "raio/codec_names.h"
#include "raio/codec_workers.h"

#define MAX_TYPES 256
#define MAX_WORKERS 64
#define MAX_PATH 10
#define MAX_EDGES 16
#define MAX_NAME 64

#define FAIL(msg) do { fprintf(stderr, "%s\n", msg); exit(1); } while (0)

typedef struct {
    uint8_t to;
    uint8_t worker;
} edge_t;

typedef struct {
    edge_t edges[MAX_EDGES];
    uint8_t count;
} node_t;

typedef struct {
    uint16_t src_to_dst;
    uint8_t workers[MAX_PATH];
    uint8_t worker_count;
} raio_pipeline_entry_t;

static node_t graph[MAX_TYPES];

int step_count(const raio_worker_t *w) {
    int c = 0;
    while (c < 3 && w->steps[c] != 0)
        c++;
    return c;
}

void build_graph(void) {
    memset(graph, 0, sizeof(graph));

    for (int w = 0; w < raio_codec_workers_len; w++) {
        int sc = step_count(&raio_codec_workers[w]);
        if (sc < 2)
            continue;
        for (int i = 0; i < sc - 1; i++) {
            uint8_t from = raio_codec_workers[w].steps[i];
            uint8_t to = raio_codec_workers[w].steps[i + 1];
            if (graph[from].count >= MAX_EDGES)
                FAIL("too many edges");
            graph[from].edges[graph[from].count++] = (edge_t){ to, w };
        }
    }
}

int find_path(uint8_t src, uint8_t dst, uint8_t *out, uint8_t *out_count) {
    int visited[MAX_TYPES];
    int prev[MAX_TYPES];
    int via[MAX_TYPES];
    memset(visited, 0, sizeof(visited));

    uint8_t queue[MAX_TYPES];
    int qh = 0, qt = 0;

    queue[qt++] = src;
    visited[src] = 1;
    prev[src] = -1;

    while (qh < qt) {
        uint8_t cur = queue[qh++];
        if (cur == dst)
            break;

        node_t *n = &graph[cur];
        for (int i = 0; i < n->count; i++) {
            edge_t *e = &n->edges[i];
            if (!visited[e->to]) {
                if (qt >= MAX_TYPES)
                    FAIL("BFS queue overflow");
                visited[e->to] = 1;
                prev[e->to] = cur;
                via[e->to] = e->worker;
                queue[qt++] = e->to;
            }
        }

        for (int w = 0; w < raio_codec_workers_len; w++) {
            if (step_count(&raio_codec_workers[w]) == 1) {
                uint8_t to = raio_codec_workers[w].steps[0];
                if (!visited[to]) {
                    if (qt >= MAX_TYPES)
                        FAIL("BFS queue overflow");
                    visited[to] = 1;
                    prev[to] = cur;
                    via[to] = w;
                    queue[qt++] = to;
                }
            }
        }
    }

    if (!visited[dst]) {
        *out_count = 0;
        return 0;
    }

    uint8_t tmp[MAX_PATH];
    int c = 0;
    int cur = dst;

    while (prev[cur] != -1) {
        if (c >= MAX_PATH)
            FAIL("path overflow");
        tmp[c++] = via[cur];
        cur = prev[cur];
    }

    for (int i = 0; i < c; i++)
        out[i] = tmp[c - 1 - i];

    *out_count = c;
    return 1;
}

const char* worker_name(uint8_t id) {
    const raio_worker_t *worker = &raio_codec_workers[id];
    raio_worker_func_t func = worker->func;
    for (int i = 0; i < raio_codec_names_len; i++) {
        if (raio_codec_names[i].func == func) {
            return raio_codec_names[i].name;
        }
    }
    return "NULL";
}

void print_lut(void) {
    printf("typedef struct {\n  uint16_t src_to_dst;\n  uint8_t worker_count;\n  uint8_t workers[%d];\n} raio_worker_path_t;\n\nraio_worker_path_t raio_codec_paths[] = {", MAX_PATH);
    for (int s = 0; s < RAIO_TYPE_COUNT; s++) {
        for (int d = 0; d < RAIO_TYPE_COUNT; d++) {
            raio_pipeline_entry_t e;
            memset(&e, 0, sizeof(e));
            e.src_to_dst = (s << 8) | d;
            find_path(s, d, e.workers, &e.worker_count);
            printf("%s/* %s -> %s ",
                (d || s)? ",\n  ": "\n  ", 
                raio_types_names[e.src_to_dst >> 8],
                raio_types_names[e.src_to_dst & 0xFF]
            );
            if (e.worker_count) {
                printf("*/\n  /* ");
                for (int i = 0; i < e.worker_count; i++)
                    printf("%s ", worker_name(e.workers[i]));
            }
            printf("*/\n  { 0x%04X, %u, {", e.src_to_dst, e.worker_count);
            for (int i = 0; i < e.worker_count; i++)
                printf("%s%u", i? ", ": " ", e.workers[i]);
            printf(" } }");
        }
    }
    printf("\n};\n\nconst unsigned int raio_codec_paths_len = %d;\n", RAIO_TYPE_COUNT * RAIO_TYPE_COUNT);
}

static const char *worker_label(int w) {
    if (raio_codec_workers[w].func == ConvertNotImplemented)
        return "not implemented";
    static char buf[16];
    snprintf(buf, sizeof(buf), "w%d", w);
    return buf;
}

void print_plantuml(void) {
    int wildcard_types[MAX_TYPES];
    int wildcard_count = 0;

    printf("/**\n@startuml\n");

    for (int w = 0; w < raio_codec_workers_len; w++) {
        if (step_count(&raio_codec_workers[w]) == 1) {
            wildcard_types[wildcard_count++] = raio_codec_workers[w].steps[0];
        }
    }

    if (wildcard_count > 0) {
        printf("rectangle Codecs {\n");
        for (int i = 0; i < RAIO_TYPE_COUNT; i++) {
            int needs_input = 1;
            for (int j = 0; j < wildcard_count; j++) {
                if (wildcard_types[j] == i) {
                    needs_input = 0;
                    break;
                }
            }
            if (needs_input)
                printf("  %s\n", raio_types_names[i]);
        }
        printf("}\n");

        for (int i = 0; i < wildcard_count; i++) {
            printf("Codecs --> %s\n", raio_types_names[wildcard_types[i]]);
        }
    }

    for (int w = 0; w < raio_codec_workers_len; w++) {
        int sc = step_count(&raio_codec_workers[w]);
        if (sc < 2)
            continue;

        for (int i = 0; i < sc - 1; i++) {
            printf("  %s --> %s : %s\n",
                   raio_types_names[raio_codec_workers[w].steps[i]],
                   raio_types_names[raio_codec_workers[w].steps[i + 1]],
                   worker_label(w));
        }
    }

    printf("@enduml\n*/\n\n");
}

int main(int argc, char **argv) {
    build_graph();
    print_plantuml();
    print_lut();
    return 0;
}

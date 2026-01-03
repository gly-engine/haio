#include <stdio.h>
#include <stdlib.h>

#include "raio.h"
#include "raio/codec_paths.h"
#include "raio/codec_names.h"
#include "raio/codec_workers.h"

static int cmpWorkers(const void *ptr_key, const void *ptr_cmp) {
    const raio_worker_path_t *worker = (const raio_worker_path_t *) ptr_cmp;
    uint16_t key = * (const uint16_t *) ptr_key;
    return (key > worker->src_to_dst) - (key < worker->src_to_dst);
}

const raio_worker_func_t *NewCodecWorkersFromFormats(raio_type_t from, raio_type_t to) {
    static const size_t size = sizeof(raio_worker_path_t);
    static const size_t len = (size_t) raio_codec_paths_len;

    uint16_t key = (from << 8) | to;
    raio_worker_path_t *path = bsearch(&key, raio_codec_paths, len, size, cmpWorkers);
    raio_worker_func_t *res = NULL;

    do {
        if (!path && path->worker_count <= 0) break;

        res = malloc(sizeof(raio_worker_func_t) * (path->worker_count + 1));

        if (!res) break;
    
        int count = 0;

        for (int i = 0; i < path->worker_count; i++) {
            uint8_t id = path->workers[i];
            if (raio_codec_workers[id].func) {
                res[count++] = raio_codec_workers[id].func;
            }
        }

        for (;count <= path->worker_count; count++) {
            res[count] = NULL;
        }
    }
    while(0);

    return res;
}

const char *const GetCodecWorkerName(raio_worker_func_t func) {
    for (int i = 0; i < raio_codec_names_len; i++) {
        if (raio_codec_names[i].func == func) {
            return raio_codec_names[i].name;
        }
    }
    return "NULL";
}

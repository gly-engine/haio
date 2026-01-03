/**
 * @file codecs.c
 * @author RodrigoDornelles
 * @date 2026-01-02
 */

#include <stdlib.h>

#include "raio.h"
#include "raio/enum_names.h"
#include "raio/codec_names.h"
#include "raio/codec_paths.h"
#include "raio/codec_workers.h"

static int cmpWorkers(const void *ptr_key, const void *ptr_cmp) {
    const raio_worker_path_t *worker = (const raio_worker_path_t *) ptr_cmp;
    uint16_t key = * (const uint16_t *) ptr_key;
    return (key > worker->src_to_dst) - (key < worker->src_to_dst);
}

/**
 * @short Find codec workers between formats
 *
 * @details This function is O(log n) because all pathfinding has already been processed at build time,
 * creating an index of routes between the source and the destination.
 *
 * @param [in] from format input
 * @param [in] to format output/intermediary
 * @param [out] func_list array of workers output
 * @param [in] size of @c func_list
 *
 * @note The final worker does not necessarily end in the @c to format,
 * but may instead be the result of an intermediate step that passes through it.
 *
 * @todo For some reason, the 3-step worker in the BFS pathfinding is being duplicated.
 * A palliative workaround was implemented (the @c last variable in the function body),
 * but this issue must be investigated more thoroughly. (eg. 1 2 2 3 4)
 *
 * @return amount of workers needed
 * @retval 0 when not found workers
 * @retval 0 when size of @c func_list is less then worker count
 */
uint8_t GetCodecWorkersFromFormats(raio_type_t from, raio_type_t to, raio_worker_func_t * func_list, uint8_t size) {
    static const size_t tsize = sizeof(raio_worker_path_t);
    static const size_t tlen = (size_t) raio_codec_paths_len;

    uint8_t last = 0;
    uint8_t count = 0;
    uint16_t key = (from << 8) | to;
    raio_worker_path_t *path = bsearch(&key, raio_codec_paths, tlen, tsize, cmpWorkers);
    raio_worker_func_t *res = NULL;

    if (!path || path->worker_count <= 0) return 0;
    if (path->worker_count < size) return 0;

    for (int i = 0; i < path->worker_count; i++) {
        uint8_t id = path->workers[i];
        if (raio_codec_workers[id].func && id != last) {
            if (func_list) func_list[count] = raio_codec_workers[id].func;
            last = id;
            count++;
        }
    }

    return count;
}

/**
 * @retval "NULL" when invalid func
 */
const char *const GetCodecWorkerName(raio_worker_func_t func) {
    for (int i = 0; i < raio_codec_names_len; i++) {
        if (raio_codec_names[i].func == func) {
            return raio_codec_names[i].name;
        }
    }
    return "NULL";
}

/**
 * @retval "NULL" when invalid format
 */
const char *const GetCodecFormatName(raio_type_t format) {
    if (RAIO_TYPE_NULL > format && format < RAIO_TYPE_COUNT) {
        return raio_types_names[format - 1];
    }
    return "NULL";
}

/**
 * @file codecs.c
 * @author RodrigoDornelles
 * @date 2026-01-02
 */

#include <stdlib.h>

#include "haio.h"
#include "haio/enum_names.h"
#include "haio/codec_names.h"
#include "haio/codec_paths.h"
#include "haio/codec_workers.h"

static int cmpWorkers(const void *ptr_key, const void *ptr_cmp) {
    const haio_worker_path_t *worker = (const haio_worker_path_t *) ptr_cmp;
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
 * @param [out] output format resulting
 *
 * @note The final worker does not necessarily end in the @c to format,
 * but may instead be the result of an intermediate step that passes through it.
 *
 * @return amount of workers needed
 * @retval 0 when not found workers
 * @retval 0 when size of @c func_list is less then worker count
 */
uint8_t GetCodecWorkersFromFormats(haio_type_t from, haio_type_t to, haio_worker_func_t * func_list, uint8_t size, haio_type_t *output) {
    static const size_t tsize = sizeof(haio_worker_path_t);
    static const size_t tlen = (size_t) haio_codec_paths_len;

    uint8_t count = 0;
    uint16_t key = (uint16_t) (((uint16_t) from << 8) | (uint16_t) to);
    haio_worker_path_t *path = bsearch(&key, haio_codec_paths, tlen, tsize, cmpWorkers);

    if (!path || path->worker_count <= 0) return 0;
    if (func_list && path->worker_count > size) return 0; 

    for (int i = 0; i < path->worker_count; i++) {
        uint8_t id = path->workers[i];
        const haio_type_t *steps = haio_codec_workers[id].steps;
        if (haio_codec_workers[id].func) {
            if (func_list) func_list[count] = haio_codec_workers[id].func;
            while (output && *++steps) *output = *steps;
            count++;
        }
    }

    return count;
}

/**
 * @retval "NULL" when invalid func
 */
const char *GetCodecWorkerName(haio_worker_func_t func) {
    for (unsigned int i = 0; i < haio_codec_names_len; i++) {
        if (haio_codec_names[i].func == func) {
            return haio_codec_names[i].name;
        }
    }
    return "NULL";
}

/**
 * @retval "NULL" when invalid format
 */
const char *GetCodecFormatName(haio_type_t format) {
    if (HAIO_TYPE_NULL < format && format < HAIO_TYPE_COUNT) {
        return haio_types_names[format - 1];
    }
    return "NULL";
}

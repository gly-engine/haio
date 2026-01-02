#include "raio.h"

// buffers:
void BufferAggregatorUntilZero(raio_worker_handle_t *handle, raio_buffer_t *src, raio_buffer_t *dst);
// conversors:
void ConvertRGBA8ToYUV420(raio_worker_handle_t *handle, raio_buffer_t *src, raio_buffer_t *dst);
void ConvertNotImplemented(raio_worker_handle_t *handle, raio_buffer_t *src, raio_buffer_t *dst);
// decoders:
void DecodePngBufferToRGBA8(raio_worker_handle_t *handle, raio_buffer_t *src, raio_buffer_t *dst);
// encoders:
void EncodeRGBA8ToPPM(raio_worker_handle_t *handle, raio_buffer_t *src, raio_buffer_t *dst);
void EncodeYUV420ToY4M(raio_worker_handle_t *handle, raio_buffer_t *src, raio_buffer_t *dst);

/**
 * @brief Workers Lookup Table describing an implicit format pipeline graph.
 *
 * Each @ref raio_worker_t entry declares a small transformation pipeline
 * using the @c steps[] array (maximum of 3 steps). The table itself represents
 * an implicit directed graph of format transitions, evaluated only at
 * build-time for code generation.
 *
 * @section steps_semantics Steps semantics
 *
 * The meaning of @c steps[] depends on @c step_count:
 *
 * - @b step_count == 1  
 *   @code
 *   { X }
 *   @endcode
 *   Accepts any input format (wildcard) and produces @c X.  
 *   Represents: @c * -> X
 *
 * - @b step_count == 2  
 *   @code
 *   { A, B }
 *   @endcode
 *   Transforms input format @c A into output format @c B.  
 *   Represents: @c A -> B
 *
 * - @b step_count == 3  
 *   @code
 *   { A, B, C }
 *   @endcode
 *   Transforms @c A into @c C through intermediate format @c B.  
 *   Represents: @c A -> B -> C
 *
 * @section graph_model Graph model
 *
 * - Graph nodes are all @c RAIO_TYPE_* values appearing in @c steps[].
 * - Graph edges are implicit transitions between consecutive steps.
 * - Workers with a single step act as wildcard producers and may be applied
 *   from any current format node.
 *
 * @section pathfinding Pathfinding
 *
 * - Pathfinding (e.g. BFS) operates on formats, not on workers.
 * - The resulting format path is reduced to an ordered list of workers.
 * - Workers may be executed partially or fully, depending on which step
 *   transition is selected.
 *
 * @note
 * Internal pipeline states are intentionally exposed so requests like
 * BUFFER -> PNG -> PPM can be resolved, even if PNG is an intermediate
 * state of a worker.
 *
 * @note
 * This structure is intended for build-time evaluation only and is not
 * optimized for runtime traversal.
 */
static const raio_worker_t workers[] = {
    {
        .steps = { RAIO_TYPE_BUFFER, RAIO_TYPE_BUFFER_FULL },
        .func = BufferAggregatorUntilZero
    },
    {
        .steps = { RAIO_TYPE_BUFFER_FULL, RAIO_TYPE_IMG_PNG, RAIO_TYPE_IMG_RGBA8 },
        .func = DecodePngBufferToRGBA8
    },
    {
        .steps = { RAIO_TYPE_IMG_RGBA8, RAIO_TYPE_IMG_PPM },
        .func = EncodeRGBA8ToPPM
    },
    {
        .steps = { RAIO_TYPE_IMG_RGBA8, RAIO_TYPE_IMG_BMP },
        .func = ConvertNotImplemented
    },
    {
        .steps = { RAIO_TYPE_IMG_RGBA8, RAIO_TYPE_IMG_YUV420 },
        .func = ConvertRGBA8ToYUV420
    },
    {
        .steps = { RAIO_TYPE_IMG_YUV420, RAIO_TYPE_IMG_Y4M420 },
        .func = EncodeYUV420ToY4M
    },
    {
        .steps = { RAIO_TYPE_BUFFER_GZIP },
        .func = ConvertNotImplemented
    }
};

static const size_t workers_len = sizeof(workers)/sizeof(raio_worker_t);

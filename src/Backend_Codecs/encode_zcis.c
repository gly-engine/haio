#include <stdio.h>

#include "haio.h"
#include "haio/functions.h"

static const char base62[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static char error_encoding[] = "zcis encoding failed";
static char error_dimensions[] = "zcis dimensions exceed base62 limit";

static void fail_zcis(haio_worker_handle_t *handle, char *error) {
    handle->error = error;
    handle->state = HAIO_FSM_WORKER_DONE;
}

static void encode_base62_pair(uint16_t value, char out[2]) {
    out[0] = base62[value / 62u];
    out[1] = base62[value % 62u];
}

static int push_ar_member_header(haio_buffer_t *dst, const char *name, size_t size) {
    char header[61];
    int count = snprintf(header, sizeof(header), "%-16s%-12u%-6u%-6u%-8o%-10zu`\n", name, 0u, 0u, 0u, 0644, size);
    if (count != 60) {
        return 1;
    }
    return BufferPush(dst, header, 60u);
}

void EncodeRGBA8ToZCIS(haio_worker_handle_t *handle, haio_buffer_t *src, haio_buffer_t *dst) {
    if (handle->state == HAIO_FSM_WORKER_DONE) {
        return;
    }

    if (handle->state == HAIO_FSM_WORKER_FINISHING) {
        handle->state = HAIO_FSM_WORKER_DONE;
        return;
    }

    if (handle->state == HAIO_FSM_WORKER_NEW) {
        if (src->len == 0) {
            return;
        }

        const haio_canvas_t *canvas = handle->canvas.parent
            ? handle->canvas.parent
            : &handle->canvas;
        uint16_t width = canvas->width;
        uint16_t height = canvas->height;
        if (width == 0u || width > 3843u || height == 0u || height > 3843u) {
            fail_zcis(handle, error_dimensions);
            return;
        }

        char zcis_header[32];
        int zcis_header_size = snprintf(zcis_header, sizeof(zcis_header), "0 0 %u %u\r\n", width, height);
        if (zcis_header_size <= 0 || (size_t) zcis_header_size >= sizeof(zcis_header)) {
            fail_zcis(handle, error_encoding);
            return;
        }

        char ppm_header[32];
        int ppm_header_size = snprintf(ppm_header, sizeof(ppm_header), "P6\n%u %u\n255\n", width, height);
        if (ppm_header_size <= 0 || (size_t) ppm_header_size >= sizeof(ppm_header)) {
            fail_zcis(handle, error_encoding);
            return;
        }

        char width_pair[2];
        char height_pair[2];
        encode_base62_pair(width, width_pair);
        encode_base62_pair(height, height_pair);

        char image_name[17];
        int image_name_size = snprintf(
            image_name,
            sizeof(image_name),
            "1A0000%c%c%c%c.ppm/",
            width_pair[0],
            width_pair[1],
            height_pair[0],
            height_pair[1]
        );
        if (image_name_size != 15) {
            fail_zcis(handle, error_encoding);
            return;
        }

        size_t pixels_size = (size_t) width * (size_t) height * 3u;
        size_t image_size = (size_t) ppm_header_size + pixels_size;

        if (BufferPush(dst, "!<arch>\n", 8u) != 0
            || push_ar_member_header(dst, "000000000000.txt", (size_t) zcis_header_size) != 0
            || BufferPush(dst, zcis_header, (size_t) zcis_header_size) != 0) {
            fail_zcis(handle, error_encoding);
            return;
        }
        if ((zcis_header_size & 1) != 0 && BufferPush(dst, "\n", 1u) != 0) {
            fail_zcis(handle, error_encoding);
            return;
        }
        if (push_ar_member_header(dst, image_name, image_size) != 0
            || BufferPush(dst, ppm_header, (size_t) ppm_header_size) != 0) {
            fail_zcis(handle, error_encoding);
            return;
        }

        handle->canvas.width = width;
        handle->canvas.height = height;
        handle->progress.nbytes.count = 0u;
        handle->progress.nbytes.total = pixels_size;
        handle->state = HAIO_FSM_WORKER_RUNNING;
    }

    size_t encoded = (src->len / 4u) * 3u;
    if (BufferPush43(dst, src->data.ptr, src->len) != 0) {
        fail_zcis(handle, error_encoding);
        return;
    }
    handle->progress.nbytes.count += encoded;

    if (handle->progress.nbytes.count >= handle->progress.nbytes.total) {
        int ppm_header_size = snprintf(
            NULL,
            0,
            "P6\n%u %u\n255\n",
            handle->canvas.width,
            handle->canvas.height
        );
        if (ppm_header_size < 0) {
            fail_zcis(handle, error_encoding);
            return;
        }
        size_t image_size = handle->progress.nbytes.total + (size_t) ppm_header_size;
        if ((image_size & 1u) != 0u && BufferPush(dst, "\n", 1u) != 0) {
            fail_zcis(handle, error_encoding);
            return;
        }
        handle->state = HAIO_FSM_WORKER_FINISHING;
    }
}

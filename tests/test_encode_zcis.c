#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "haio.h"
#include "haio/functions.h"

static size_t member_size(const uint8_t *header) {
    char field[11];
    memcpy(field, header + 48, 10u);
    field[10] = '\0';
    return (size_t) strtoul(field, NULL, 10);
}

int main(void) {
    static const haio_canvas_t canvas = {
        .width = 8u,
        .height = 1u
    };
    uint8_t pixels[32];
    uint8_t expected[24];
    for (size_t index = 0; index < 8u; index++) {
        pixels[index * 4u] = (uint8_t) (index * 3u + 1u);
        pixels[index * 4u + 1u] = (uint8_t) (index * 3u + 2u);
        pixels[index * 4u + 2u] = (uint8_t) (index * 3u + 3u);
        pixels[index * 4u + 3u] = 255u;
        expected[index * 3u] = pixels[index * 4u];
        expected[index * 3u + 1u] = pixels[index * 4u + 1u];
        expected[index * 3u + 2u] = pixels[index * 4u + 2u];
    }

    haio_buffer_t src = {
        .len = sizeof(pixels),
        .size = sizeof(pixels),
        .data.u8 = pixels
    };
    haio_buffer_t dst = {0};
    haio_worker_handle_t handle = {0};
    handle.canvas = canvas;

    EncodeRGBA8ToZCIS(&handle, &src, &dst);

    assert(handle.state == HAIO_FSM_WORKER_FINISHING);
    assert(handle.error == NULL);
    assert(dst.len == 174u);
    assert(memcmp(dst.data.u8, "!<arch>\n", 8u) == 0);

    const uint8_t *header = dst.data.u8 + 8u;
    assert(memcmp(header, "000000000000.txt", 16u) == 0);
    size_t header_size = member_size(header);
    assert(header_size == 9u);
    assert(memcmp(header + 60u, "0 0 8 1\r\n", header_size) == 0);

    const uint8_t *image_header = header + 60u + header_size + 1u;
    assert(memcmp(image_header, "1A00000801.ppm/", 15u) == 0);
    size_t image_size = member_size(image_header);
    assert(image_size == 35u);
    assert(memcmp(image_header + 60u, "P6\n8 1\n255\n", 11u) == 0);
    assert(memcmp(image_header + 71u, expected, sizeof(expected)) == 0);

    BufferClose(&dst);
    return 0;
}

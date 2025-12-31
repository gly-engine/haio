#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "img.h"

#define BUFFER_SIZE 1024

int FrontendConvertCli(int argc, char* argv[]) {
    if (argc <= 2) {
        printf("usage:\n./img convert input.png output.ppm\n");
        return 1;
    }

    char* input_path = argv[1];
    char* output_path = argv[2];

    const raio_worker_t* worker_png_to_rgba = GetPipelineWorker(RAIO_TYPE_IMG_PNG, RAIO_TYPE_IMG_RGBA8);
    const raio_worker_t* worker_argb_to_ppm = GetPipelineWorker(RAIO_TYPE_IMG_RGBA8, RAIO_TYPE_IMG_PPM);

    if (worker_png_to_rgba == NULL || worker_argb_to_ppm == NULL) {
        fprintf(stderr, "Failed to create conversion pipeline.\nAre the required workers compiled in?\n");
        return 1;
    }

    static raio_worker_handle_t worker_ctx[2];
    static raio_buffer_t worker_buf[3];

    worker_buf[0].data.ptr = malloc(BUFFER_SIZE);
    worker_buf[0].size = BUFFER_SIZE;
    worker_buf[1].data.ptr = malloc(512);
    worker_buf[1].size = 512;
    worker_buf[2].data.ptr = malloc(BUFFER_SIZE);
    worker_buf[2].size = BUFFER_SIZE;

    FILE* f_in = fopen(input_path, "rb");
    FILE* f_out = fopen(output_path, "rb");

    worker_ctx[1].state = RAIO_FSM_WORKER_DONE;

    while(worker_ctx[0].state != RAIO_FSM_WORKER_DONE || worker_ctx[1].state != RAIO_FSM_WORKER_DONE) {
        worker_buf[0].len = fread(worker_buf[0].data.str, 1, worker_buf[0].size, f_in);

        printf("buffer 0 %ld/%ld\n", worker_buf[0].len, worker_buf[0].size);
        worker_png_to_rgba->func(&worker_ctx[0], &worker_buf[0], &worker_buf[1]);
        printf("buffer 1 %ld/%ld\n", worker_buf[1].len, worker_buf[1].size);
        worker_ctx[1].width = worker_ctx[0].width;
        worker_ctx[1].height = worker_ctx[0].height;
        worker_argb_to_ppm->func(&worker_ctx[1], &worker_buf[1], &worker_buf[2]);
        printf("buffer 2 %ld/%ld\n", worker_buf[2].len, worker_buf[2].size);

        printf("bytes escritos: %ld\n", fwrite(worker_buf[2].data.ptr, 1, worker_buf[2].len, f_out));
    }

    fclose(f_in);
    fclose(f_out);

    return 0;
}

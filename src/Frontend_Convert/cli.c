#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "img.h"

int FrontendConvertCli(int argc, char* argv[]) {
    if (argc <= 2) {
        printf("usage:\n./img convert input.png output.ppm\n");
        return 1;
    }

    char* input_path = argv[1];
    char* output_path = argv[2];

    const raio_worker_t* worker_png_to_argb = GetPipelineWorker(RAIO_TYPE_IMG_PNG, RAIO_TYPE_IMG_ARGB_8888);
    const raio_worker_t* worker_argb_to_ppm = GetPipelineWorker(RAIO_TYPE_IMG_ARGB_8888, RAIO_TYPE_IMG_PPM);

    if (worker_png_to_argb == NULL || worker_argb_to_ppm == NULL) {
        fprintf(stderr, "Failed to create conversion pipeline.\nAre the required workers compiled in?\n");
        return 1;
    }

    static raio_worker_handle_t worker_ctx[2];
    memset(worker_ctx, 0, sizeof(worker_ctx));

    static raio_buffer_t worker_buf[3];
    memset(worker_buf, 0, sizeof(worker_buf));

    // --- Read input file ---
    FILE* f_in = fopen(input_path, "rb");
    if (!f_in) {
        perror("fopen input");
        return 1;
    }

    fseek(f_in, 0, SEEK_END);
    worker_buf[0].len = ftell(f_in);
    fseek(f_in, 0, SEEK_SET);

    worker_buf[0].data.u8 = malloc(worker_buf[0].len);
    worker_buf[0].capacity = worker_buf[0].len;

    if (!worker_buf[0].data.u8) {
        fprintf(stderr, "Failed to allocate memory for input file.\n");
        fclose(f_in);
        return 1;
    }

    if (fread(worker_buf[0].data.u8, 1, worker_buf[0].len, f_in) != worker_buf[0].len) {
        fprintf(stderr, "Failed to read input file.\n");
        fclose(f_in);
        free(worker_buf[0].data.u8);
        return 1;
    }
    fclose(f_in);

    // --- Execute pipeline ---

    // 1. PNG -> ARGB
    worker_png_to_argb->func(&worker_ctx[0], &worker_buf[0], &worker_buf[1]);
    if (worker_buf[1].len == 0) {
        fprintf(stderr, "PNG to ARGB conversion failed.\n");
        free(worker_buf[0].data.u8);
        if (worker_buf[1].data.u8) free(worker_buf[1].data.u8);
        return 1;
    }

    // Pass image dimensions
    worker_ctx[1].width = worker_ctx[0].width;
    worker_ctx[1].height = worker_ctx[0].height;

    // 2. ARGB -> PPM
    worker_argb_to_ppm->func(&worker_ctx[1], &worker_buf[1], &worker_buf[2]);
    if (worker_buf[2].len == 0) {
        fprintf(stderr, "ARGB to PPM conversion failed.\n");
        free(worker_buf[0].data.u8);
        free(worker_buf[1].data.u8);
        if (worker_buf[2].data.u8) free(worker_buf[2].data.u8);
        return 1;
    }

    // --- Write output file ---
    FILE* f_out = fopen(output_path, "wb");
    if (!f_out) {
        perror("fopen output");
        free(worker_buf[0].data.u8);
        free(worker_buf[1].data.u8);
        free(worker_buf[2].data.u8);
        return 1;
    }

    if (fwrite(worker_buf[2].data.u8, 1, worker_buf[2].len, f_out) != worker_buf[2].len) {
        fprintf(stderr, "Failed to write to output file.\n");
    }

    fclose(f_out);

    // --- Cleanup ---
    free(worker_buf[0].data.u8);
    free(worker_buf[1].data.u8);
    free(worker_buf[2].data.u8);

    printf("File converted successfully: %s -> %s\n", input_path, output_path);

    return 0;
}

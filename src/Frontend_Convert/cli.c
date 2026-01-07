#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raio.h"
#include "raio/functions.h"

#define BUFFER_SIZE 1024

int FrontendConvertCli(int argc, char* argv[]) {
    if (argc <= 2) {
        printf("usage:\n./img convert input.png output.ppm\n");
        return 1;
    }

    size_t nbytes;
    char buffer[4096];
    FILE* f_in = fopen(argv[1], "rb");
    FILE* f_out = fopen(argv[2], "wb");

    raio_pipeline_t pipe;
    PipelineBegin(&pipe, RAIO_TYPE_BUFFER);
    PipelineStepAdd(&pipe, RAIO_TYPE_IMG_PNG);
    PipelineEnd(&pipe, RAIO_TYPE_IMG_PPM);

    while(PipelineIsRunning(&pipe)) {
        nbytes = fread(buffer, sizeof(char), sizeof(buffer), f_in);
        nbytes = PipelineProcess(&pipe, buffer, sizeof(buffer), buffer, sizeof(buffer));
        fwrite(buffer, sizeof(char), nbytes, f_out);
    }

    if (PipelineHasError(&pipe)) {
        printf("[error] %s", GetPipelineError(&pipe));
        return 1;
    }

    printf("feito!");

    fclose(f_in);
    fclose(f_out);

    return 0;
}

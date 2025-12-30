#include <stdio.h>
#include <stdlib.h>

#include "img.h"

int FrontendConvertCli(int argc, char* argv[]) {
    if (argc <= 2) {
        printf("usage:\n./img convert input.png output.png\n");
        return 1;
    }

    raio_pipeline_t *pipeline = BackendPipelineFromUrl(argv[1]);
    BackendPipelineToUrl(pipeline, argv[2]);

    return 0;
}

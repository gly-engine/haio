#include <stdio.h>

#include "haio.h"
#include "haio/functions.h"

#define BUFFER_SIZE 16384

int FrontendConvertCli(int argc, char* argv[]) {
    if (argc <= 2) {
        fprintf(stderr, "usage: haio convert input.png output.zcis\n");
        return 1;
    }

    haio_type_t input_format = GetFormatFromExtension(GetExtensionFromString(argv[1]));
    haio_type_t output_format = GetFormatFromExtension(GetExtensionFromString(argv[2]));
    if (input_format == HAIO_TYPE_NULL || output_format == HAIO_TYPE_NULL) {
        fprintf(stderr, "unsupported file extension\n");
        return 1;
    }
    if (output_format != HAIO_TYPE_IMG_PPM && output_format != HAIO_TYPE_IMG_ZCIS) {
        fprintf(stderr, "unsupported conversion\n");
        return 1;
    }

    haio_pipeline_t pipe;
    PipelineBegin(&pipe, HAIO_TYPE_BUFFER);
    PipelineStepAdd(&pipe, input_format);
    PipelineEnd(&pipe, output_format);
    if (pipe.current_format != output_format) {
        fprintf(stderr, "unsupported conversion\n");
        return 1;
    }

    FILE* f_in = fopen(argv[1], "rb");
    if (!f_in) {
        fprintf(stderr, "could not open input: %s\n", argv[1]);
        return 1;
    }

    FILE* f_out = fopen(argv[2], "wb");
    if (!f_out) {
        fprintf(stderr, "could not open output: %s\n", argv[2]);
        fclose(f_in);
        return 1;
    }

    char buffer[BUFFER_SIZE];
    int result = 0;
    while (PipelineIsRunning(&pipe)) {
        size_t input_nbytes = fread(buffer, sizeof(char), sizeof(buffer), f_in);
        size_t output_nbytes = PipelineProcess(&pipe, buffer, input_nbytes, buffer, sizeof(buffer));
        if (output_nbytes > 0 && fwrite(buffer, sizeof(char), output_nbytes, f_out) != output_nbytes) {
            fprintf(stderr, "could not write output: %s\n", argv[2]);
            result = 1;
            break;
        }
    }
    if (ferror(f_in)) {
        fprintf(stderr, "could not read input: %s\n", argv[1]);
        result = 1;
    }

    if (PipelineHasError(&pipe)) {
        fprintf(stderr, "[error] %s\n", GetPipelineError(&pipe));
        result = 1;
    }
    PipelineClean(&pipe);

    fclose(f_in);
    fclose(f_out);
    return result;
}

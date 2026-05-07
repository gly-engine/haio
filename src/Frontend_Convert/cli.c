#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "haio.h"
#include "haio/functions.h"

#define BUFFER_SIZE 4096
#define FORMAT_PROBE_SIZE 16

static void PrintConvertUsage(void)
{
    printf("usage:\n");
    printf("./img convert input.png [options] output.ppm\n");
    printf("\n");
    printf("options:\n");
    printf("  -crop    crop image using the default crop filter\n");
}

static haio_type_t GetFormatFromPath(char *path)
{
    char *ext = GetExtensionFromString(path);

    if (!ext) {
        return HAIO_TYPE_NULL;
    }

    return GetFormatFromExtension(ext);
}

static haio_type_t GetInputFormatFromBytes(const unsigned char *buffer, size_t len)
{
    static const unsigned char png_signature[] = {
        0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'
    };

    if (len >= sizeof(png_signature) &&
        memcmp(buffer, png_signature, sizeof(png_signature)) == 0) {
        return HAIO_TYPE_IMG_PNG;
    }

    return HAIO_TYPE_NULL;
}

static haio_type_t GetInputFormat(FILE *f_in, char *path)
{
    unsigned char buffer[FORMAT_PROBE_SIZE];
    haio_type_t format;
    size_t nread;

    nread = fread(buffer, sizeof(unsigned char), sizeof(buffer), f_in);
    format = GetInputFormatFromBytes(buffer, nread);

    if (fseek(f_in, 0, SEEK_SET)) {
        return HAIO_TYPE_NULL;
    }

    if (format != HAIO_TYPE_NULL) {
        return format;
    }

    return GetFormatFromPath(path);
}

static int PipelineAddConvertToken(haio_pipeline_t *pipe, char *token)
{
    if (strcmp(token, "-crop") == 0) {
        return PipelineStepAdd(pipe, HAIO_TYPE_FILTER_CROP);
    }

    printf("[error] unknown token: %s\n", token);
    return 1;
}

static int PipelineParseConvertArgs(
    haio_pipeline_t *pipe,
    int argc,
    char* argv[],
    haio_type_t input_format,
    haio_type_t output_format
)
{
    PipelineBegin(pipe, HAIO_TYPE_BUFFER);

    if (PipelineStepAdd(pipe, input_format)) {
        printf("[error] %s\n", GetPipelineError(pipe));
        return 1;
    }

    for (int i = 2; i < argc - 1; i++) {
        if (PipelineAddConvertToken(pipe, argv[i])) {
            if (PipelineHasError(pipe)) {
                printf("[error] %s\n", GetPipelineError(pipe));
            }
            return 1;
        }
    }

    if (PipelineEnd(pipe, output_format)) {
        printf("[error] %s\n", GetPipelineError(pipe));
        return 1;
    }

    return 0;
}

int FrontendConvertCli(int argc, char* argv[])
{
    size_t nread;
    size_t nwrite;
    char buffer[BUFFER_SIZE];

    FILE* f_in;
    FILE* f_out;

    haio_type_t input_format;
    haio_type_t output_format;
    haio_pipeline_t pipe;

    if (argc <= 2) {
        PrintConvertUsage();
        return 1;
    }

    output_format = GetFormatFromPath(argv[argc - 1]);
    if (output_format == HAIO_TYPE_NULL) {
        printf("[error] unknown output format: %s\n", argv[argc - 1]);
        return 1;
    }

    f_in = fopen(argv[1], "rb");
    if (!f_in) {
        printf("[error] could not open input: %s\n", argv[1]);
        return 1;
    }

    input_format = GetInputFormat(f_in, argv[1]);
    if (input_format == HAIO_TYPE_NULL) {
        printf("[error] unknown input format: %s\n", argv[1]);
        fclose(f_in);
        return 1;
    }

    if (PipelineParseConvertArgs(&pipe, argc, argv, input_format, output_format)) {
        fclose(f_in);
        return 1;
    }

    if (PipelineHasError(&pipe)) {
        printf("[error] %s\n", GetPipelineError(&pipe));
        fclose(f_in);
        return 1;
    }

    f_out = fopen(argv[argc - 1], "wb");
    if (!f_out) {
        fclose(f_in);
        printf("[error] could not open output: %s\n", argv[argc - 1]);
        return 1;
    }

    while(PipelineIsRunning(&pipe)) {
        nread = fread(buffer, sizeof(unsigned char), sizeof(buffer), f_in);

        nwrite = PipelineProcess(&pipe, buffer, nread, buffer, sizeof(buffer));

        if (nwrite && fwrite(buffer, sizeof(char), nwrite, f_out) != nwrite) {
            printf("[error] could not write output: %s\n", argv[argc - 1]);
            fclose(f_in);
            fclose(f_out);
            return 1;
        }
    }

    if (ferror(f_in)) {
        printf("[error] could not read input: %s\n", argv[1]);
        fclose(f_in);
        fclose(f_out);
        return 1;
    }

    if (PipelineHasError(&pipe)) {
        printf("[error] %s\n", GetPipelineError(&pipe));
        fclose(f_in);
        fclose(f_out);
        return 1;
    }

    fclose(f_in);
    fclose(f_out);

    return 0;
}

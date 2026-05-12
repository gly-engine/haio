#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "haio.h"
#include "haio/functions.h"
#include "convert.h"

#define BUFFER_SIZE 4096

static void PrintConvertUsage(void)
{
    printf("usage:\n");
    printf("haio convert input.png [options] output.ppm\n");
    printf("haio convert png:- output.ppm\n");
    printf("\n");
    printf("options:\n");
    printf("  -crop    crop image using the default crop filter\n");
}

static void PrintConvertBuildError(convert_command_t *cmd, haio_pipeline_t *pipe)
{
    if (cmd->error.message) {
        convert_print_error(cmd->error);
        return;
    }

    if (PipelineHasError(pipe)) {
        printf("[error] %s\n", GetPipelineError(pipe));
    }
}

static int OpenConvertInput(convert_command_t *cmd, FILE **f_in)
{
    if (cmd->has_generator) {
        *f_in = NULL;
        return 0;
    }

    if (cmd->input_format_name && cmd->input_format == HAIO_TYPE_NULL) {
        convert_print_unsupported_format("input", cmd->input_format_name, cmd->input_path);
        return 1;
    }

    if (strcmp(cmd->input_path, "-") == 0) {
        *f_in = stdin;
        return 0;
    }

    *f_in = fopen(cmd->input_path, "rb");
    if (!*f_in) {
        printf("[error] could not open input: %s\n", cmd->input_path);
        return 1;
    }

    if (!cmd->input_format_name) {
        cmd->input_format = convert_input_format_from_file(*f_in, cmd->input_path);
    }

    if (cmd->input_format == HAIO_TYPE_NULL) {
        convert_print_unsupported_format("input", cmd->input_format_name, cmd->input_path);
        fclose(*f_in);
        *f_in = NULL;
        return 1;
    }

    return 0;
}

static int OpenConvertOutput(convert_command_t *cmd, FILE **f_out)
{
    if (cmd->output_is_stdout) {
        *f_out = stdout;
        return 0;
    }

    *f_out = fopen(cmd->output_path, "wb");
    if (!*f_out) {
        printf("[error] could not open output: %s\n", cmd->output_path);
        return 1;
    }

    return 0;
}

int FrontendConvertCli(int argc, char* argv[])
{
    int err = 1;
    size_t nread;
    size_t nwrite;
    char buffer[BUFFER_SIZE];

    FILE* f_in = NULL;
    FILE* f_out = NULL;

    convert_command_t cmd;
    haio_pipeline_t pipe;

    if (argc <= 2) {
        PrintConvertUsage();
        goto defer;
    }

    if (convert_tokenize_args(&cmd, argc, argv)) {
        convert_print_error(cmd.error);
        goto defer;
    }

    if (cmd.output_format == HAIO_TYPE_NULL) {
        convert_print_unsupported_format("output", cmd.output_format_name, argv[argc - 1]);
        goto defer;
    }

    if (OpenConvertInput(&cmd, &f_in)) {
        goto defer;
    }

    if (convert_build_pipeline(&cmd, &pipe)) {
        PrintConvertBuildError(&cmd, &pipe);
        goto defer;
    }

    if (OpenConvertOutput(&cmd, &f_out)) {
        goto defer;
    }

    while(PipelineIsRunning(&pipe)) {
        nread = fread(buffer, sizeof(char), sizeof(buffer), f_in);

        nwrite = PipelineProcess(&pipe, buffer, nread, buffer, sizeof(buffer));

        if (nwrite && fwrite(buffer, sizeof(char), nwrite, f_out) != nwrite) {
            printf("[error] could not write output: %s\n", cmd.output_path);
            goto defer;
        }
    }

    if (ferror(f_in)) {
        printf("[error] could not read input: %s\n", cmd.input_path);
        goto defer;
    }

    if (PipelineHasError(&pipe)) {
        printf("[error] %s\n", GetPipelineError(&pipe));
        goto defer;
    }

    err = 0;

defer:
    if (f_in && f_in != stdin) {
        fclose(f_in);
    }

    if (f_out && f_out != stdout) {
        fclose(f_out);
    }

    return err;
}

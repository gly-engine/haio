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
    int err = 1;

    do {
        if (cmd->has_generator) {
            *f_in = NULL;
            err = 0;
            break;
        }

        if (cmd->input_format_name && cmd->input_format == HAIO_TYPE_NULL) {
            convert_print_unsupported_format("input", cmd->input_format_name, cmd->input_path);
            break;
        }

        if (strcmp(cmd->input_path, "-") == 0) {
            *f_in = stdin;
            err = 0;
            break;
        }

        *f_in = fopen(cmd->input_path, "rb");
        if (!*f_in) {
            printf("[error] could not open input: %s\n", cmd->input_path);
            break;
        }

        if (!cmd->input_format_name) {
            cmd->input_format = convert_input_format_from_file(*f_in, cmd->input_path);
        }

        if (cmd->input_format == HAIO_TYPE_NULL) {
            convert_print_unsupported_format("input", cmd->input_format_name, cmd->input_path);
            fclose(*f_in);
            *f_in = NULL;
            break;
        }

        err = 0;
    }
    while(0);

    return err;
}

static int OpenConvertOutput(convert_command_t *cmd, FILE **f_out)
{
    int err = 1;

    do {
        if (cmd->output_is_stdout) {
            *f_out = stdout;
            err = 0;
            break;
        }

        *f_out = fopen(cmd->output_path, "wb");
        if (!*f_out) {
            printf("[error] could not open output: %s\n", cmd->output_path);
            break;
        }

        err = 0;
    }
    while(0);

    return err;
}

int FrontendConvertCli(int argc, char* argv[])
{
    int err = 1;
    int write_error = 0;
    size_t nread;
    size_t nwrite;
    char buffer[BUFFER_SIZE];

    FILE* f_in = NULL;
    FILE* f_out = NULL;

    convert_command_t cmd;
    haio_pipeline_t pipe;

    do {
        if (argc <= 2) {
            PrintConvertUsage();
            break;
        }

        if (convert_tokenize_args(&cmd, argc, argv)) {
            convert_print_error(cmd.error);
            break;
        }

        if (cmd.output_format == HAIO_TYPE_NULL) {
            convert_print_unsupported_format("output", cmd.output_format_name, argv[argc - 1]);
            break;
        }

        if (OpenConvertInput(&cmd, &f_in)) {
            break;
        }

        if (convert_build_pipeline(&cmd, &pipe)) {
            PrintConvertBuildError(&cmd, &pipe);
            break;
        }

        if (OpenConvertOutput(&cmd, &f_out)) {
            break;
        }

        while(PipelineIsRunning(&pipe)) {
            nread = fread(buffer, sizeof(char), sizeof(buffer), f_in);

            nwrite = PipelineProcess(&pipe, buffer, nread, buffer, sizeof(buffer));

            if (nwrite && fwrite(buffer, sizeof(char), nwrite, f_out) != nwrite) {
                printf("[error] could not write output: %s\n", cmd.output_path);
                write_error = 1;
                break;
            }
        }

        if (write_error) {
            break;
        }

        if (ferror(f_in)) {
            printf("[error] could not read input: %s\n", cmd.input_path);
            break;
        }

        if (PipelineHasError(&pipe)) {
            printf("[error] %s\n", GetPipelineError(&pipe));
            break;
        }

        err = 0;
    }
    while(0);

    if (f_in && f_in != stdin) {
        fclose(f_in);
    }

    if (f_out && f_out != stdout) {
        fclose(f_out);
    }

    return err;
}

#include <stdint.h>

#include "haio.h"
#include "haio/functions.h"
#include "convert.h"

static void set_error(convert_command_t *cmd, char *message, char *token)
{
    cmd->error.message = message;
    cmd->error.token = token;
}

static char *unsupported_token_error(convert_token_type_t type)
{
    static char unsupported_generator[] = "generator inputs are not supported yet";
    static char unsupported_fx[] = "-fx is not supported by the pipeline yet";
    static char unsupported_crop_geometry[] = "-crop geometry is not supported by the pipeline yet";
    static char unsupported_token[] = "unsupported convert token";

    switch (type) {
        case CONVERT_TOKEN_GENERATOR_XC:
        case CONVERT_TOKEN_GENERATOR_GRADIENT:
            return unsupported_generator;
        case CONVERT_TOKEN_FILTER_FX:
            return unsupported_fx;
        case CONVERT_TOKEN_FILTER_CROP:
            return unsupported_crop_geometry;
        case CONVERT_TOKEN_INPUT_FILE:
        case CONVERT_TOKEN_OUTPUT_FILE:
            break;
    }

    return unsupported_token;
}

int convert_build_pipeline(convert_command_t *cmd, haio_pipeline_t *pipe)
{
    static char unknown_input_format[] = "unknown input format";
    static char unknown_output_format[] = "unknown output format";

    if (cmd->has_generator) {
        set_error(cmd, unsupported_token_error(CONVERT_TOKEN_GENERATOR_XC), NULL);
        return 1;
    }

    if (cmd->input_format == HAIO_TYPE_NULL) {
        set_error(cmd, unknown_input_format, cmd->input_path);
        return 1;
    }

    if (cmd->output_format == HAIO_TYPE_NULL) {
        set_error(cmd, unknown_output_format, cmd->output_path);
        return 1;
    }

    PipelineBegin(pipe, HAIO_TYPE_BUFFER);

    if (PipelineStepAdd(pipe, cmd->input_format)) {
        return 1;
    }

    for (uint8_t i = 0; i < cmd->token_count; i++) {
        convert_token_t *token = &cmd->tokens[i];

        switch (token->type) {
            case CONVERT_TOKEN_FILTER_CROP:
                if (token->value) {
                    set_error(cmd, unsupported_token_error(token->type), token->value);
                    return 1;
                }

                if (PipelineStepAdd(pipe, HAIO_TYPE_FILTER_CROP)) {
                    return 1;
                }
                break;
            case CONVERT_TOKEN_INPUT_FILE:
            case CONVERT_TOKEN_OUTPUT_FILE:
                break;
            case CONVERT_TOKEN_GENERATOR_XC:
            case CONVERT_TOKEN_GENERATOR_GRADIENT:
            case CONVERT_TOKEN_FILTER_FX:
                set_error(cmd, unsupported_token_error(token->type), token->value);
                return 1;
        }
    }

    return PipelineEnd(pipe, cmd->output_format);
}

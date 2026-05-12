#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "haio.h"
#include "convert.h"

static bool string_equals(char *txt, const char *expected)
{
    return strcmp(txt, expected) == 0;
}

static void convert_command_init(convert_command_t *cmd)
{
    memset(cmd, 0, sizeof(convert_command_t));
}

static void convert_set_error(convert_command_t *cmd, char *message, char *token)
{
    cmd->error.message = message;
    cmd->error.token = token;
}

void convert_print_error(convert_error_t error)
{
    if (!error.message) {
        return;
    }

    if (error.token) {
        printf("[error] %s: %s\n", error.message, error.token);
        return;
    }

    printf("[error] %s\n", error.message);
}

static bool known_source_prefix(char *name, size_t len)
{
    return convert_slice_iequals(name, len, "xc") ||
        convert_slice_iequals(name, len, "canvas") ||
        convert_slice_iequals(name, len, "gradient") ||
        convert_slice_iequals(name, len, "radial-gradient");
}

static bool is_crop_geometry(char *token)
{
    char *p = token;
    uint8_t offset_count = 0;

    if (!isdigit((unsigned char)*p)) {
        return false;
    }

    while (isdigit((unsigned char)*p)) {
        p++;
    }

    if (*p != 'x') {
        return false;
    }

    p++;

    if (!isdigit((unsigned char)*p)) {
        return false;
    }

    while (isdigit((unsigned char)*p)) {
        p++;
    }

    while (*p == '+' || *p == '-') {
        offset_count++;
        p++;

        if (!isdigit((unsigned char)*p)) {
            return false;
        }

        while (isdigit((unsigned char)*p)) {
            p++;
        }
    }

    return *p == '\0' && offset_count != 1;
}

static int token_add(
    convert_command_t *cmd,
    convert_token_type_t type,
    char *value,
    char *arg,
    haio_type_t format
)
{
    static char too_many_tokens[] = "too many convert tokens";

    if (cmd->token_count >= CONVERT_MAX_TOKENS) {
        convert_set_error(cmd, too_many_tokens, value);
        return 1;
    }

    cmd->tokens[cmd->token_count] = (convert_token_t) {
        .type = type,
        .value = value,
        .arg = arg,
        .format = format
    };

    cmd->token_count++;

    return 0;
}

static int tokenize_generator(
    convert_command_t *cmd,
    convert_token_type_t type,
    char *value,
    char *size,
    char *token
)
{
    static char multiple_inputs[] = "multiple convert inputs are not supported yet";

    if (cmd->has_input) {
        convert_set_error(cmd, multiple_inputs, token);
        return 1;
    }

    cmd->has_input = true;
    cmd->has_generator = true;

    return token_add(cmd, type, value, size, HAIO_TYPE_NULL);
}

static int tokenize_input_file(convert_command_t *cmd, char *token)
{
    static char multiple_inputs[] = "multiple convert inputs are not supported yet";
    static char missing_format_path[] = "missing path after format prefix";
    char *sep = convert_find_format_separator(token);
    haio_type_t format = HAIO_TYPE_NULL;

    if (cmd->has_input) {
        convert_set_error(cmd, multiple_inputs, token);
        return 1;
    }

    cmd->has_input = true;

    if (sep && convert_known_format_prefix(token, (size_t)(sep - token))) {
        if (!sep[1]) {
            convert_set_error(cmd, missing_format_path, token);
            return 1;
        }

        format = convert_format_from_name(token, (size_t)(sep - token));
        cmd->input_path = sep + 1;
        cmd->input_format_name = token;
        cmd->input_format = format;
    } else {
        cmd->input_path = token;
        cmd->input_format = convert_format_from_path(token);
    }

    return token_add(
        cmd,
        CONVERT_TOKEN_INPUT_FILE,
        cmd->input_path,
        cmd->input_format_name,
        cmd->input_format
    );
}

static int tokenize_output_file(convert_command_t *cmd, char *token)
{
    static char missing_format_path[] = "missing path after format prefix";
    char *sep = convert_find_format_separator(token);
    haio_type_t format = HAIO_TYPE_NULL;

    if (sep && convert_known_format_prefix(token, (size_t)(sep - token))) {
        if (!sep[1]) {
            convert_set_error(cmd, missing_format_path, token);
            return 1;
        }

        format = convert_format_from_name(token, (size_t)(sep - token));
        cmd->output_path = sep + 1;
        cmd->output_format_name = token;
        cmd->output_format = format;
    } else {
        cmd->output_path = token;
        cmd->output_format = convert_format_from_path(token);
    }

    cmd->output_is_stdout = string_equals(cmd->output_path, "-");

    return token_add(
        cmd,
        CONVERT_TOKEN_OUTPUT_FILE,
        cmd->output_path,
        cmd->output_format_name,
        cmd->output_format
    );
}

static int tokenize_source(convert_command_t *cmd, char *token, char *size)
{
    char *sep = convert_find_format_separator(token);

    if (!sep) {
        return tokenize_input_file(cmd, token);
    }

    if (convert_slice_iequals(token, (size_t)(sep - token), "xc") ||
        convert_slice_iequals(token, (size_t)(sep - token), "canvas")) {
        return tokenize_generator(
            cmd,
            CONVERT_TOKEN_GENERATOR_XC,
            sep + 1,
            size,
            token
        );
    }

    if (convert_slice_iequals(token, (size_t)(sep - token), "gradient") ||
        convert_slice_iequals(token, (size_t)(sep - token), "radial-gradient")) {
        return tokenize_generator(
            cmd,
            CONVERT_TOKEN_GENERATOR_GRADIENT,
            sep + 1,
            size,
            token
        );
    }

    return tokenize_input_file(cmd, token);
}

int convert_tokenize_args(convert_command_t *cmd, int argc, char* argv[])
{
    static char missing_input[] = "missing convert input";
    static char missing_option_arg[] = "missing convert option argument";
    static char unknown_option[] = "unknown convert option";
    static char size_without_generator[] = "-size must be followed by a generator source";
    char *pending_size = NULL;

    convert_command_init(cmd);

    if (argc <= 2) {
        convert_set_error(cmd, missing_input, NULL);
        return 1;
    }

    for (int i = 1; i < argc - 1; i++) {
        char *token = argv[i];

        if (string_equals(token, "-size")) {
            if ((i + 1) >= argc - 1) {
                convert_set_error(cmd, missing_option_arg, token);
                return 1;
            }

            pending_size = argv[++i];
            continue;
        }

        if (string_equals(token, "-fx")) {
            if (pending_size) {
                convert_set_error(cmd, size_without_generator, pending_size);
                return 1;
            }

            if ((i + 1) >= argc - 1) {
                convert_set_error(cmd, missing_option_arg, token);
                return 1;
            }

            if (token_add(
                cmd,
                CONVERT_TOKEN_FILTER_FX,
                argv[++i],
                NULL,
                HAIO_TYPE_NULL
            )) {
                return 1;
            }

            pending_size = NULL;
            continue;
        }

        if (string_equals(token, "-crop")) {
            char *crop = NULL;

            if (pending_size) {
                convert_set_error(cmd, size_without_generator, pending_size);
                return 1;
            }

            if ((i + 1) < argc - 1 && is_crop_geometry(argv[i + 1])) {
                crop = argv[++i];
            }

            if (token_add(
                cmd,
                CONVERT_TOKEN_FILTER_CROP,
                crop,
                NULL,
                HAIO_TYPE_FILTER_CROP
            )) {
                return 1;
            }

            pending_size = NULL;
            continue;
        }

        if (token[0] == '-' && !string_equals(token, "-")) {
            convert_set_error(cmd, unknown_option, token);
            return 1;
        }

        if (pending_size) {
            char *sep = convert_find_format_separator(token);

            if (!sep || !known_source_prefix(token, (size_t)(sep - token))) {
                convert_set_error(cmd, size_without_generator, pending_size);
                return 1;
            }
        }

        if (tokenize_source(cmd, token, pending_size)) {
            return 1;
        }

        pending_size = NULL;
    }

    if (pending_size) {
        convert_set_error(cmd, size_without_generator, pending_size);
        return 1;
    }

    if (!cmd->has_input) {
        convert_set_error(cmd, missing_input, NULL);
        return 1;
    }

    if (tokenize_output_file(cmd, argv[argc - 1])) {
        return 1;
    }

    return 0;
}

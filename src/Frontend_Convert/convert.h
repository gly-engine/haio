#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "haio.h"

#define CONVERT_MAX_TOKENS 32
#define CONVERT_FORMAT_PROBE_SIZE 16

typedef enum {
    CONVERT_TOKEN_INPUT_FILE,
    CONVERT_TOKEN_OUTPUT_FILE,
    CONVERT_TOKEN_GENERATOR_XC,
    CONVERT_TOKEN_GENERATOR_GRADIENT,
    CONVERT_TOKEN_FILTER_CROP,
    CONVERT_TOKEN_FILTER_FX
} convert_token_type_t;

typedef struct {
    convert_token_type_t type;
    char *value;
    char *arg;
    haio_type_t format;
} convert_token_t;

typedef struct {
    char *message;
    char *token;
} convert_error_t;

typedef struct {
    convert_error_t error;
    uint8_t token_count;
    bool has_input;
    bool has_generator;
    bool output_is_stdout;
    char *input_path;
    char *output_path;
    char *input_format_name;
    char *output_format_name;
    haio_type_t input_format;
    haio_type_t output_format;
    convert_token_t tokens[CONVERT_MAX_TOKENS];
} convert_command_t;

haio_type_t convert_format_from_path(char *path);
haio_type_t convert_input_format_from_bytes(const unsigned char *buffer, size_t len);
haio_type_t convert_input_format_from_file(FILE *f_in, char *path);
void convert_print_unsupported_format(const char *kind, char *format_name, char *fallback);
int convert_tokenize_args(convert_command_t *cmd, int argc, char* argv[]);
int convert_build_pipeline(convert_command_t *cmd, haio_pipeline_t *pipe);
void convert_print_error(convert_error_t error);

bool convert_slice_iequals(char *txt, size_t len, const char *expected);
char *convert_find_format_separator(char *txt);
haio_type_t convert_format_from_name(char *name, size_t len);
bool convert_known_format_prefix(char *name, size_t len);

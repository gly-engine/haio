#include <assert.h>
#include <string.h>

#include "haio.h"
#include "../src/Frontend_Convert/convert.h"

int main(void)
{
    convert_command_t cmd;
    {
        char *argv[] = { "convert", "input.png", "-crop", "output.ppm" };
        assert(!convert_tokenize_args(&cmd, 4, argv));
        assert(cmd.token_count == 3);
        assert(cmd.tokens[0].type == CONVERT_TOKEN_INPUT_FILE);
        assert(cmd.tokens[1].type == CONVERT_TOKEN_FILTER_CROP);
        assert(cmd.tokens[2].type == CONVERT_TOKEN_OUTPUT_FILE);
        assert(cmd.input_format == HAIO_TYPE_IMG_PNG);
        assert(cmd.output_format == HAIO_TYPE_IMG_PPM);
    }
    {
        char *argv[] = {
            "convert", "-size", "512x512", "xc:white", "-fx", "j/h", "out.png"
        };
        assert(!convert_tokenize_args(&cmd, 7, argv));
        assert(cmd.token_count == 3);
        assert(cmd.has_generator);
        assert(cmd.tokens[0].type == CONVERT_TOKEN_GENERATOR_XC);
        assert(strcmp(cmd.tokens[0].value, "white") == 0);
        assert(strcmp(cmd.tokens[0].arg, "512x512") == 0);
        assert(cmd.tokens[1].type == CONVERT_TOKEN_FILTER_FX);
        assert(strcmp(cmd.tokens[1].value, "j/h") == 0);
        assert(cmd.tokens[2].type == CONVERT_TOKEN_OUTPUT_FILE);
        assert(cmd.output_format == HAIO_TYPE_IMG_PNG);
    }
    {
        char *argv[] = { "convert", "input.png", "ppm:-" };
        assert(!convert_tokenize_args(&cmd, 3, argv));
        assert(cmd.output_is_stdout);
        assert(strcmp(cmd.output_path, "-") == 0);
        assert(cmd.output_format == HAIO_TYPE_IMG_PPM);
    }
    {
        char *argv[] = { "convert", "png:-", "ppm:-" };
        assert(!convert_tokenize_args(&cmd, 3, argv));
        assert(strcmp(cmd.input_path, "-") == 0);
        assert(cmd.input_format == HAIO_TYPE_IMG_PNG);
        assert(cmd.output_is_stdout);
    }
    {
        char *argv[] = { "convert", "PNG:-", "PPM:-" };
        assert(!convert_tokenize_args(&cmd, 3, argv));
        assert(cmd.input_format == HAIO_TYPE_IMG_PNG);
        assert(cmd.output_format == HAIO_TYPE_IMG_PPM);
        assert(cmd.output_is_stdout);
    }
    {
        char *argv[] = { "convert", "foo:bar.png", "out.ppm" };
        assert(!convert_tokenize_args(&cmd, 3, argv));
        assert(strcmp(cmd.input_path, "foo:bar.png") == 0);
        assert(cmd.input_format == HAIO_TYPE_IMG_PNG);
    }
    {
        char *argv[] = { "convert", "input.png", "foo:out.ppm" };
        assert(!convert_tokenize_args(&cmd, 3, argv));
        assert(strcmp(cmd.output_path, "foo:out.ppm") == 0);
        assert(cmd.output_format == HAIO_TYPE_IMG_PPM);
    }
    {
        char *argv[] = { "convert", "input.png", "jpeg:-" };
        assert(!convert_tokenize_args(&cmd, 3, argv));
        assert(cmd.output_is_stdout);
        assert(strcmp(cmd.output_path, "-") == 0);
        assert(cmd.output_format_name != NULL);
        assert(cmd.output_format == HAIO_TYPE_NULL);
    }
    {
        char *argv[] = { "convert", "jpeg:renamed.png", "out.ppm" };
        assert(!convert_tokenize_args(&cmd, 3, argv));
        assert(strcmp(cmd.input_path, "renamed.png") == 0);
        assert(cmd.input_format_name != NULL);
        assert(cmd.input_format == HAIO_TYPE_NULL);
    }
    {
        char *argv[] = { "convert", "png:", "out.ppm" };
        assert(convert_tokenize_args(&cmd, 3, argv));
        assert(cmd.error.message != NULL);
        assert(strcmp(cmd.error.token, "png:") == 0);
    }
    {
        char *argv[] = { "convert", "png:-", "ppm:" };
        assert(convert_tokenize_args(&cmd, 3, argv));
        assert(cmd.error.message != NULL);
        assert(strcmp(cmd.error.token, "ppm:") == 0);
    }
    {
        char *argv[] = { "convert", "input.png", "-crop", "other.png", "out.ppm" };
        assert(convert_tokenize_args(&cmd, 5, argv));
        assert(cmd.error.message != NULL);
        assert(strcmp(cmd.error.token, "other.png") == 0);
    }
    {
        char *argv[] = { "convert", "-size", "out.ppm" };
        assert(convert_tokenize_args(&cmd, 3, argv));
        assert(cmd.error.message != NULL);
    }
    {
        char *argv[] = { "convert", "-size", "512x512", "-crop", "xc:white", "out.ppm" };
        assert(convert_tokenize_args(&cmd, 6, argv));
        assert(cmd.error.message != NULL);
    }
    {
        char *argv[] = { "convert", "input.png", "-wat", "out.ppm" };
        assert(convert_tokenize_args(&cmd, 4, argv));
        assert(cmd.error.message != NULL);
        assert(strcmp(cmd.error.token, "-wat") == 0);
    }
    return 0;
}

#include <haio/parser.hpp>

namespace Haio::Parser {

std::expected<Pipeline, Error> build(const Parsed& parsed) {
    if (parsed.error) return std::unexpected(parsed.error);
    for (const auto& input : parsed.inputs) {
        if (input.format == Format::RAW) return std::unexpected(Error{"no decode delegate for this image format " + Stages::quoted(input.path), input.path});
    }

    /**
     * "raw:" is the container that is not one: the pixels as they are, in whatever
     * -pix_fmt asked for. it has to be named rather than guessed, because a file
     * extension never means it, which is exactly what tells it apart from a format
     * nobody recognised.
     */
    if (parsed.outputFormat == Format::RAW && parsed.outputFormatName.empty()) {
        return std::unexpected(Error{"no encode delegate for this image format " + Stages::quoted(parsed.outputPath), parsed.outputPath});
    }

    // each stage built its own token, so the steps go in as they came
    Pipeline pipeline;
    for (const auto& token : parsed.steps) {
        if (token.kind == TokenKind::Fx) return std::unexpected(Error{"unsupported option `-fx', read and not yet run: " + token.expression, token.expression});
        pipeline |= token;
    }
    pipeline |= Tokens::Encode(parsed.outputFormat, parsed.outputColor, parsed.outputSettings);
    return pipeline;
}

}

#include <haio/implementation/language/parser.hpp>
#include <haio/util/option.hpp>

namespace Haio::Implementation::LanguageParser {

std::expected<Language::Pipeline, ParseError> build(const Command& parsed) {
    if (parsed.error) return std::unexpected(parsed.error);
    for (const auto& input : parsed.inputs) {
        if (input.format == Format::RAW) return std::unexpected(ParseError{"no decode delegate for this image format " + Util::quoted(input.path), input.path});
    }

    /**
     * "raw:" is the container that is not one: the pixels as they are, in whatever
     * -pix_fmt asked for. it has to be named rather than guessed, because a file
     * extension never means it, which is exactly what tells it apart from a format
     * nobody recognised.
     */
    if (parsed.outputFormat == Format::RAW && parsed.outputFormatName.empty()) {
        return std::unexpected(ParseError{"no encode delegate for this image format " + Util::quoted(parsed.outputPath), parsed.outputPath});
    }

    // each stage built its own token, so the steps go in as they came
    Language::Pipeline pipeline;
    for (const auto& token : parsed.steps) {
        if (token.kind == Language::TokenKind::Fx) return std::unexpected(ParseError{"unsupported option `-fx', read and not yet run: " + token.expression, token.expression});
        pipeline |= token;
    }
    pipeline |= LanguageToken::Encode(parsed.outputFormat, parsed.outputColor, parsed.outputSettings);
    return pipeline;
}

}

#include <haio_cli.hpp>

#include <stdexcept>

namespace Haio::Cli {
namespace {

std::runtime_error tokenError(std::string message, const std::string& token = {}) {
    if (!token.empty()) message += ": " + token;
    return std::runtime_error(std::move(message));
}

}

Pipeline buildPipeline(const Command& command) {
    if (command.error) throw tokenError(command.error.message, command.error.token);
    if (command.hasGenerator) throw tokenError("generator inputs are not supported yet");
    if (command.inputFormat == Format::RAW) throw tokenError("unknown input format", command.inputPath);

    /**
     * "raw:" is the container that is not one: the pixels as they are, in whatever
     * -pix_fmt asked for. it has to be named rather than guessed, because a file
     * extension never means it, which is exactly what tells it apart from a format
     * nobody recognised.
     */
    if (command.outputFormat == Format::RAW && command.outputFormatName.empty()) {
        throw tokenError("unknown output format", command.outputPath);
    }

    Pipeline pipeline;
    pipeline |= Tokens::Source("file", command.inputPath);
    pipeline |= Tokens::Decode(command.inputFormat);

    for (const auto& token : command.tokens) {
        switch (token.type) {
            case TokenType::FilterCrop:
                if (!token.rect) throw tokenError("default -crop is not supported yet", token.value);
                pipeline |= Tokens::Crop(*token.rect);
                break;
            case TokenType::FilterResize:
                // a share carries no size, and is not meant to: what it comes to is
                // not known until there is a picture to take a share of
                if (token.percent != 0) {
                    pipeline |= Tokens::ResizeByPercent(token.percent);
                    break;
                }
                if (!token.size) throw tokenError("invalid resize size", token.value);
                pipeline |= Tokens::Resize(*token.size);
                break;
            case TokenType::FilterPalette:
                pipeline |= Tokens::Palette(token.value, token.dither, token.limit, token.limitHow);
                break;

            case TokenType::FilterRadius:
                pipeline |= Tokens::Radius(token.radius);
                break;
            case TokenType::FilterFx:
                throw tokenError("-fx is not supported by the pipeline yet", token.value);
            case TokenType::GeneratorXc:
            case TokenType::GeneratorGradient:
                throw tokenError("generator inputs are not supported yet", token.value);
            case TokenType::InputFile:
            case TokenType::OutputFile:
            case TokenType::FilterFormat:
            // both of these are answered by the encode token below rather than in
            // place: they say what comes out, and nothing comes out until the end
            case TokenType::FilterPixFmt:
                break;
        }
    }

    pipeline |= Tokens::Encode(command.outputFormat, command.outputColor);
    return pipeline;
}

} // namespace Haio::Cli

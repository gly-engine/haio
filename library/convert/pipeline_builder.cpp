#include <haio_convert.hpp>

#include <stdexcept>

namespace Haio::Convert {
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
    if (command.outputFormat == Format::RAW) throw tokenError("unknown output format", command.outputPath);

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
                if (!token.size) throw tokenError("invalid resize size", token.value);
                pipeline |= Tokens::Resize(*token.size);
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
                break;
        }
    }

    pipeline |= Tokens::Encode(command.outputFormat);
    return pipeline;
}

} // namespace Haio::Convert

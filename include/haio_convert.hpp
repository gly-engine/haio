#pragma once

#include "haio.hpp"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Haio::Convert {

enum class TokenType {
    InputFile,
    OutputFile,
    GeneratorXc,
    GeneratorGradient,
    FilterCrop,
    FilterResize,
    FilterRadius,
    FilterFormat,
    FilterFx,
};

struct Error {
    std::string message;
    std::string token;

    explicit operator bool() const noexcept;
};

struct Token {
    TokenType type = TokenType::InputFile;
    std::string value;
    std::string arg;
    Format format = Format::RAW;
    std::optional<Rect> rect;
    std::optional<Size> size;
    int radius = 0;
};

struct Command {
    Error error;
    bool hasInput = false;
    bool hasGenerator = false;
    bool outputIsStdout = false;
    std::string inputPath;
    std::string outputPath;
    std::string inputFormatName;
    std::string outputFormatName;
    Format inputFormat = Format::RAW;
    Format outputFormat = Format::RAW;
    std::vector<Token> tokens;
};

Command parseArgs(int argc, char* argv[]);
Command parseCommandLine(std::string_view text);
std::vector<std::string> lexCommandLine(std::string_view text);
Pipeline buildPipeline(const Command& command);
int runCli(int argc, char* argv[]);

bool parseSizeToken(std::string_view text, Size& out);
bool parseCropGeometryToken(std::string_view text, Rect& out);
bool parseRectToken(std::string_view text, Rect& out);

}

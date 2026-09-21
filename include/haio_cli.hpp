#pragma once

#include "haio.hpp"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/**
 * the convert command line.
 *
 * it used to be namespace Haio::Convert, which now belongs to the colour conversion
 * family -- a name cannot be a namespace and a function template at once.
 */
namespace Haio::Cli {

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
    FilterPalette,
};

struct ParseError {
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
    int percent = 0;
    Dither dither = Dither::Nearest;
    size_t limit = 0;   /**< zero means the palette is used whole */
    Limit limitHow = Limit::Spread;
};

struct Command {
    ParseError error;

    /**
     * an option that has been read and not yet used by anything.
     *
     * settings stack and operations take them, so "-filter bayer -palete cga" works
     * and "-palete cga -filter bayer" does not: by the time the filter is written
     * down the palette has already gone by, and nothing after it wants one. a setting
     * still waiting when the line ends is a mistake worth stopping for, because it
     * was written in the belief that it would do something.
     */
    struct Pending {
        std::string option;
        std::string value;
    };
    std::optional<Pending> pendingFilter;
    std::optional<Pending> pendingLimit;
    Limit pendingLimitHow = Limit::Spread;   /**< only meaningful when pendingLimit is set */

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

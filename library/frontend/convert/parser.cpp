#include <haio_cli.hpp>
#include <haio_cli_grammar.hpp>
#include <haio_string.hpp>

#include <boost/spirit/home/x3.hpp>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <vector>

namespace x3 = boost::spirit::x3;

namespace Haio::Cli {
namespace {

struct PrefixSpec {
    std::string prefix;
    std::string value;
};

std::string lower(std::string_view value) {
    std::string out(value);
    std::ranges::transform(out, out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

bool isGeneratorPrefix(std::string_view prefix) {
    const auto key = lower(prefix);
    return key == "xc" || key == "canvas" || key == "gradient" || key == "radial-gradient";
}

std::optional<Format> knownFormat(std::string_view name) {
    try {
        const auto format = formatFromName(name);
        if (format == Format::RAW && !lower(name).starts_with("raw")) return std::nullopt;
        return format;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<PrefixSpec> parsePrefixedSpec(std::string_view token) {
    PrefixSpec spec;
    auto first = token.begin();
    const auto last = token.end();

    const auto prefixRule = x3::lexeme[+(x3::char_ - ':' - '/' - '\\')];
    const auto valueRule = x3::lexeme[*x3::char_];
    if (!x3::parse(first, last, prefixRule, spec.prefix)) return std::nullopt;
    if (!x3::parse(first, last, x3::lit(':'))) return std::nullopt;
    if (!x3::parse(first, last, valueRule, spec.value) || first != last) return std::nullopt;

    if (spec.prefix.empty() || spec.value.empty()) return spec;
    if (!knownFormat(spec.prefix) && !isGeneratorPrefix(spec.prefix)) return std::nullopt;
    return spec;
}

Format formatFromPathOrRaw(std::string_view path) {
    try {
        return formatFromExtension(path);
    } catch (const std::exception&) {
        return Format::RAW;
    }
}

void setError(Command& command, std::string message, std::string token = {}) {
    command.error.message = std::move(message);
    command.error.token = std::move(token);
}

bool isOption(std::string_view token) {
    return token.starts_with('-') && token != "-";
}

std::string unescapeDoubleQuoted(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            out.push_back(text[++i]);
        } else {
            out.push_back(text[i]);
        }
    }
    return out;
}

std::string normalizeLexeme(std::string text) {
    if (text == "\\(") return "(";
    if (text == "\\)") return ")";
    if (text.size() >= 2 && text.front() == '\'' && text.back() == '\'') {
        return text.substr(1, text.size() - 2);
    }
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        return unescapeDoubleQuoted(std::string_view(text).substr(1, text.size() - 2));
    }
    return text;
}

TokenType generatorType(std::string_view prefix) {
    const auto key = lower(prefix);
    return key == "gradient" || key == "radial-gradient" ? TokenType::GeneratorGradient : TokenType::GeneratorXc;
}

bool addToken(Command& command, Token token) {
    constexpr size_t maxTokens = 32;
    if (command.tokens.size() >= maxTokens) {
        setError(command, "too many convert tokens", token.value);
        return false;
    }
    command.tokens.push_back(std::move(token));
    return true;
}

bool tokenizeGenerator(Command& command, const PrefixSpec& spec, std::string sizeArg, std::string token) {
    if (command.hasInput) {
        setError(command, "multiple convert inputs are not supported yet", token);
        return false;
    }

    command.hasInput = true;
    command.hasGenerator = true;
    return addToken(command, Token{.type = generatorType(spec.prefix), .value = spec.value, .arg = std::move(sizeArg)});
}

bool tokenizeInput(Command& command, std::string token) {
    if (command.hasInput) {
        setError(command, "multiple convert inputs are not supported yet", token);
        return false;
    }

    command.hasInput = true;
    command.inputPath = token;
    command.inputFormat = formatFromPathOrRaw(token);

    if (const auto spec = parsePrefixedSpec(token); spec && knownFormat(spec->prefix)) {
        if (spec->value.empty()) {
            setError(command, "missing path after format prefix", token);
            return false;
        }
        command.inputPath = spec->value;
        command.inputFormatName = spec->prefix;
        command.inputFormat = *knownFormat(spec->prefix);
    }

    return addToken(command, Token{.type = TokenType::InputFile, .value = command.inputPath, .arg = command.inputFormatName, .format = command.inputFormat});
}

bool tokenizeOutput(Command& command, std::string token) {
    command.outputPath = token;

    if (const auto spec = parsePrefixedSpec(token); spec && knownFormat(spec->prefix)) {
        if (spec->value.empty()) {
            setError(command, "missing path after format prefix", token);
            return false;
        }
        command.outputPath = spec->value;
        command.outputFormatName = spec->prefix;
        command.outputFormat = *knownFormat(spec->prefix);
    } else if (command.outputFormatName.empty()) {
        command.outputFormat = formatFromPathOrRaw(token);
    }

    command.outputIsStdout = command.outputPath == "-";

    return addToken(command, Token{.type = TokenType::OutputFile, .value = command.outputPath, .arg = command.outputFormatName, .format = command.outputFormat});
}

bool tokenizeSource(Command& command, std::string token, std::string pendingSize) {
    if (const auto spec = parsePrefixedSpec(token); spec && isGeneratorPrefix(spec->prefix)) {
        return tokenizeGenerator(command, *spec, std::move(pendingSize), std::move(token));
    }

    /**
     * -size belongs to a generator, the way imagemagick means it: it is the size
     * something is created at, and a file already has one. scaling a file is -resize,
     * which is a different question and says so.
     */
    if (!pendingSize.empty()) {
        setError(command, "-size is the size a generator is created at; use -resize to scale a picture",
                 pendingSize);
        return false;
    }

    return tokenizeInput(command, std::move(token));
}

std::string requireOptionValue(Command& command, size_t& i, std::span<const std::string> args, std::string_view token, std::string_view option) {
    const auto prefix = std::string(option) + '=';
    if (token.starts_with(prefix)) return std::string(token.substr(prefix.size()));
    if (i + 1 >= args.size()) {
        setError(command, "missing convert option argument", std::string(option));
        return {};
    }
    return args[++i];
}

bool addCrop(Command& command, std::string value, bool required) {
    std::optional<Rect> rect;
    if (!value.empty()) {
        Rect parsed;
        if (!parseCropGeometryToken(value, parsed) && !parseRectToken(value, parsed)) {
            setError(command, "invalid crop geometry", value);
            return false;
        }
        rect = parsed;
    } else if (required) {
        setError(command, "missing convert option argument", "-crop");
        return false;
    }

    return addToken(command, Token{.type = TokenType::FilterCrop, .value = std::move(value), .format = Format::RAW, .rect = rect});
}

bool addResize(Command& command, std::string value, std::string option) {
    // a share first, since "30%" is not a size and never parses as one
    if (const auto share = String::getPercent(value); share != 0) {
        return addToken(command, Token{.type = TokenType::FilterResize, .value = std::move(value),
                                       .format = Format::RAW, .percent = share});
    }

    Size size;
    if (!parseSizeToken(value, size)) {
        setError(command, "invalid resize size, which is either WxH or a share such as 30% or 30pct",
                 value.empty() ? option : value);
        return false;
    }
    return addToken(command, Token{.type = TokenType::FilterResize, .value = std::move(value), .format = Format::RAW, .size = size});
}

/** a count, which has to be a whole positive number and nothing else */
std::optional<size_t> countOf(const std::string& value) {
    try {
        const auto number = String::getInt(value);
        if (number <= 0) return std::nullopt;
        return static_cast<size_t>(number);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

bool addRadius(Command& command, std::string value) {
    int radius = 0;
    try {
        radius = String::getInt(value);
    } catch (const std::exception&) {
        setError(command, "invalid radius", value);
        return false;
    }
    return addToken(command, Token{.type = TokenType::FilterRadius, .value = std::move(value), .format = Format::RAW, .radius = radius});
}

bool addFormat(Command& command, std::string value) {
    const auto format = knownFormat(value);
    if (!format) {
        setError(command, "unknown output format", value);
        return false;
    }
    command.outputFormat = *format;
    command.outputFormatName = value;
    return addToken(command, Token{.type = TokenType::FilterFormat, .value = std::move(value), .format = *format});
}

bool addFx(Command& command, std::string value) {
    return addToken(command, Token{.type = TokenType::FilterFx, .value = std::move(value)});
}

Command parseTokens(std::span<const std::string> args) {
    Command command;
    if (args.size() <= 2) {
        setError(command, "missing convert input");
        return command;
    }

    std::string pendingSize;
    bool hasOutput = false;

    for (size_t i = 1; i < args.size(); i++) {
        const std::string_view token = args[i];

        if (token == "-size") {
            pendingSize = requireOptionValue(command, i, args, token, "-size");
            if (command.error) return command;
            continue;
        }

        if (token == "-fx" || token.starts_with("-fx=")) {
            auto value = requireOptionValue(command, i, args, token, "-fx");
            if (command.error || !addFx(command, std::move(value))) return command;
            continue;
        }

        if (token == "-crop") {
            std::string value;
            if (i + 1 < args.size() && !isOption(args[i + 1])) {
                Rect ignored;
                const std::string_view next = args[i + 1];
                if (parseCropGeometryToken(next, ignored) || parseRectToken(next, ignored)) {
                    value = args[++i];
                } else if (i + 2 < args.size()) {
                    setError(command, "invalid crop geometry", std::string(next));
                    return command;
                }
            }

            if (!addCrop(command, std::move(value), false)) return command;
            continue;
        }

        if (token == "--crop" || token.starts_with("--crop=")) {
            auto value = requireOptionValue(command, i, args, token, "--crop");
            if (command.error || !addCrop(command, std::move(value), true)) return command;
            continue;
        }

        if (token == "--resize" || token.starts_with("--resize=") || token == "-resize" || token.starts_with("-resize=")) {
            const auto option = token.starts_with("--resize") ? "--resize" : "-resize";
            auto value = requireOptionValue(command, i, args, token, option);
            if (command.error || !addResize(command, std::move(value), option)) return command;
            continue;
        }

        /**
         * a setting: it waits for an operation that wants it, and is an error if none
         * comes. this is what makes the order mean something rather than being
         * decoration.
         */
        if (token == "--filter" || token.starts_with("--filter=") || token == "-filter" || token.starts_with("-filter=")) {
            const auto option = token.starts_with("--filter") ? "--filter" : "-filter";
            auto value = requireOptionValue(command, i, args, token, option);
            if (command.error) return command;

            if (!Haio::ditherNamed(value)) {
                command.error = ParseError{"filter takes nearest, bayer, floyd or error", value};
                return command;
            }
            if (command.pendingFilter) {
                command.error = ParseError{"this filter replaces one nothing has used yet",
                                           command.pendingFilter->value};
                return command;
            }
            command.pendingFilter = Command::Pending{option, std::move(value)};
            continue;
        }

        /** another setting, and this one is optional rather than required */
        if (token == "--limit" || token.starts_with("--limit=") || token == "-limit" || token.starts_with("-limit=")) {
            const auto option = token.starts_with("--limit") ? "--limit" : "-limit";
            auto value = requireOptionValue(command, i, args, token, option);
            if (command.error) return command;

            /**
             * the strategy is written out, the same way the filter is.
             *
             * "sort:16" and "spread:16" give visibly different pictures of the same
             * photograph: one spends the budget where there is the most area, the
             * other where there is the most that is new. there is no answer that is
             * right often enough to be assumed, and a bare "16" would be haio
             * choosing what somebody's picture looks like.
             */
            const auto colon = std::string_view{value}.find(':');
            if (colon == std::string_view::npos) {
                command.error = ParseError{"limit needs to say which colours to keep, "
                                           "as in -limit spread:16 or -limit sort:16", value};
                return command;
            }

            const auto named = Haio::limitNamed(std::string_view{value}.substr(0, colon));
            if (!named) {
                command.error = ParseError{"limit takes sort or spread before its colon", value};
                return command;
            }
            command.pendingLimitHow = *named;

            const auto most = countOf(std::string(std::string_view{value}.substr(colon + 1)));
            if (!most) {
                command.error = ParseError{"limit takes a number of colours, counted from one", value};
                return command;
            }
            if (command.pendingLimit) {
                command.error = ParseError{"this limit replaces one nothing has used yet",
                                           command.pendingLimit->value};
                return command;
            }
            command.pendingLimit = Command::Pending{option, std::move(value)};
            continue;
        }

        /** an operation: it takes the filter waiting for it, and needs one */
        if (token == "--palete" || token.starts_with("--palete=") || token == "-palete" || token.starts_with("-palete=")
            || token == "--palette" || token.starts_with("--palette=") || token == "-palette" || token.starts_with("-palette=")) {
            const auto option = token.starts_with("--pal") ? "--palete" : "-palete";
            auto value = requireOptionValue(command, i, args, token, option);
            if (command.error) return command;

            if (!command.pendingFilter) {
                command.error = ParseError{"putting a picture into a palette needs a filter first, "
                                           "as in -filter bayer before -palete", value};
                return command;
            }
            const auto how = Haio::ditherNamed(command.pendingFilter->value);
            command.pendingFilter.reset();

            size_t most = 0;
            auto limitHow = Haio::Limit::Spread;
            if (command.pendingLimit) {
                const auto spelled = std::string_view{command.pendingLimit->value};
                most = *countOf(std::string(spelled.substr(spelled.find(':') + 1)));
                limitHow = command.pendingLimitHow;
                command.pendingLimit.reset();
            }

            if (!addToken(command, Token{.type = TokenType::FilterPalette, .value = std::move(value),
                                         .format = Format::RAW, .dither = *how,
                                         .limit = most, .limitHow = limitHow})) {
                return command;
            }
            continue;
        }

        if (token == "--radius" || token.starts_with("--radius=") || token == "-radius" || token.starts_with("-radius=")) {
            const auto option = token.starts_with("--radius") ? "--radius" : "-radius";
            auto value = requireOptionValue(command, i, args, token, option);
            if (command.error || !addRadius(command, std::move(value))) return command;
            continue;
        }

        if (token == "--format" || token.starts_with("--format=") || token == "-format" || token.starts_with("-format=")) {
            const auto option = token.starts_with("--format") ? "--format" : "-format";
            auto value = requireOptionValue(command, i, args, token, option);
            if (command.error || !addFormat(command, std::move(value))) return command;
            continue;
        }

        if (isOption(token)) {
            setError(command, "unknown convert option", std::string(token));
            return command;
        }

        if (!command.hasInput) {
            if (!tokenizeSource(command, std::string(token), std::move(pendingSize))) return command;
            pendingSize.clear();
            continue;
        }

        if (!pendingSize.empty()) {
            setError(command, "-size is the size a generator is created at; use -resize to scale a picture",
                     pendingSize);
            return command;
        }

        if (hasOutput) {
            setError(command, "multiple convert outputs are not supported yet", std::string(token));
            return command;
        }

        if (!tokenizeOutput(command, std::string(token))) return command;
        hasOutput = true;
    }

    // nothing followed it, so no generator was ever created at that size
    if (!pendingSize.empty()) {
        setError(command, "nothing used this -size; it has to come before a generator such as xc:",
                 pendingSize);
        return command;
    }

    if (!command.hasInput) {
        setError(command, "missing convert input");
        return command;
    }

    if (!hasOutput) {
        setError(command, "missing convert output");
        return command;
    }

    return command;
}

} // namespace

ParseError::operator bool() const noexcept {
    return !message.empty();
}

bool parseSizeToken(std::string_view text, Size& out) {
    return String::tryGetSize(text, out);
}

bool parseCropGeometryToken(std::string_view text, Rect& out) {
    return String::tryGetCropGeometry(text, out);
}

bool parseRectToken(std::string_view text, Rect& out) {
    return String::tryGetRect(text, out);
}

std::vector<std::string> lexCommandLine(std::string_view text) {
    std::vector<std::string> args;
    auto first = text.begin();
    const auto last = text.end();
    if (!x3::parse(first, last, Lexer::cmdline, args) || first != last) {
        throw std::runtime_error("invalid convert command line");
    }

    std::ranges::transform(args, args.begin(), normalizeLexeme);
    return args;
}

Command parseCommandLine(std::string_view text) {
    return parseTokens(lexCommandLine(text));
}

Command parseArgs(int argc, char* argv[]) {
    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(std::max(argc, 0)));
    for (int i = 0; i < argc; i++) {
        args.emplace_back(argv[i]);
    }

    auto command = parseTokens(args);
    if (command.error) return command;

    /**
     * the line is over and something is still waiting to be used.
     *
     * it is checked here rather than where it was written, because a setting is not
     * wrong when it is read: it is wrong only once the line ends without anything
     * having taken it, which is exactly what "-scale 200% -filter point" does.
     */
    for (const auto* waiting : {&command.pendingFilter, &command.pendingLimit}) {
        if (!*waiting) continue;
        command.error = ParseError{"nothing used this " + (*waiting)->option
                                       + "; it has to come before what it applies to",
                                   (*waiting)->value};
        return command;
    }
    return command;
}

} // namespace Haio::Cli

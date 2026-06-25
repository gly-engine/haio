#include <haio_convert.hpp>
#include <haio_convert_grammar.hpp>
#include <haio_string.hpp>

#include <boost/spirit/home/x3.hpp>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <vector>

namespace x3 = boost::spirit::x3;

namespace Haio::Convert {
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

    if (!pendingSize.empty()) {
        setError(command, "-size must be followed by a generator source", pendingSize);
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

    return addToken(command, Token{.type = TokenType::FilterCrop, .value = std::move(value), .format = Format::RGBA8888, .rect = rect});
}

bool addResize(Command& command, std::string value, std::string option) {
    Size size;
    if (!parseSizeToken(value, size)) {
        setError(command, "invalid resize size", value.empty() ? option : value);
        return false;
    }
    return addToken(command, Token{.type = TokenType::FilterResize, .value = std::move(value), .format = Format::RGBA8888, .size = size});
}

bool addRadius(Command& command, std::string value) {
    int radius = 0;
    try {
        radius = String::getInt(value);
    } catch (const std::exception&) {
        setError(command, "invalid radius", value);
        return false;
    }
    return addToken(command, Token{.type = TokenType::FilterRadius, .value = std::move(value), .format = Format::RGBA8888, .radius = radius});
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
            if (!pendingSize.empty()) {
                setError(command, "-size must be followed by a generator source", pendingSize);
                return command;
            }
            auto value = requireOptionValue(command, i, args, token, "-fx");
            if (command.error || !addFx(command, std::move(value))) return command;
            continue;
        }

        if (token == "-crop") {
            if (!pendingSize.empty()) {
                setError(command, "-size must be followed by a generator source", pendingSize);
                return command;
            }

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

        if (token == "--size" || token.starts_with("--size=") || token == "--resize" || token.starts_with("--resize=") || token == "-resize" || token.starts_with("-resize=")) {
            const auto option = token.starts_with("--size") ? "--size" : token.starts_with("--resize") ? "--resize" : "-resize";
            auto value = requireOptionValue(command, i, args, token, option);
            if (command.error || !addResize(command, std::move(value), option)) return command;
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
            setError(command, "-size must be followed by a generator source", pendingSize);
            return command;
        }

        if (hasOutput) {
            setError(command, "multiple convert outputs are not supported yet", std::string(token));
            return command;
        }

        if (!tokenizeOutput(command, std::string(token))) return command;
        hasOutput = true;
    }

    if (!pendingSize.empty()) {
        setError(command, "-size must be followed by a generator source", pendingSize);
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

Error::operator bool() const noexcept {
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
    return parseTokens(args);
}

} // namespace Haio::Convert

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
    return std::ranges::find(Lexer::generatorKinds, key) != std::ranges::end(Lexer::generatorKinds);
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

/**
 * the words an option takes.
 *
 * the table counts them, so this is the same work whatever the option is: what
 * followed the equals sign, or the next word along. -crop is the one that can do
 * without, and it looks before it takes, so it never comes through here.
 */
std::optional<std::string> valueOf(Command& command, const Lexer::Word& word, size_t& i,
                                   std::span<const std::string> args) {
    if (word.hasValue) return std::string(word.value);
    if (word.option->args == 0) return std::string{};
    if (i + 1 >= args.size()) {
        setError(command, "missing convert option argument", std::string(word.spelling));
        return std::nullopt;
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

/**
 * the colour stored inside the output, which is not the same question as the
 * container: a tga holds any of six and a ktx2 any of three, and until now the answer
 * was whichever one the registry happened to try first.
 */
bool addPixelFormat(Command& command, std::string value) {
    const auto color = colorNamed(value);
    if (!color) {
        setError(command, "unknown pixel format, which is a colour such as rgba8888, bgr888, rgb565 or yuv420", value);
        return false;
    }

    command.outputColor = *color;
    command.outputColorName = value;
    return addToken(command, Token{.type = TokenType::FilterPixFmt, .value = std::move(value), .color = *color});
}

bool addFx(Command& command, std::string value) {
    return addToken(command, Token{.type = TokenType::FilterFx, .value = std::move(value)});
}

/**
 * a setting: it waits for the operation that wants it, and is an error if none comes.
 * this is what makes the order mean something rather than being decoration.
 */
bool addFilter(Command& command, std::string value, std::string option) {
    if (!Haio::ditherNamed(value)) {
        command.error = ParseError{"filter takes nearest, bayer, floyd or strict", value};
        return false;
    }
    if (command.pendingFilter) {
        command.error = ParseError{"this filter replaces one nothing has used yet",
                                   command.pendingFilter->value};
        return false;
    }
    command.pendingFilter = Command::Pending{std::move(option), std::move(value)};
    return true;
}

/** another setting, and this one is optional rather than required */
bool addLimit(Command& command, std::string value, std::string option) {
    /**
     * the strategy is written out, the same way the filter is.
     *
     * "sort:16" and "spread:16" give visibly different pictures of the same
     * photograph: one spends the budget where there is the most area, the other where
     * there is the most that is new. there is no answer that is right often enough to
     * be assumed, and a bare "16" would be haio choosing what somebody's picture
     * looks like.
     */
    const auto colon = std::string_view{value}.find(':');
    if (colon == std::string_view::npos) {
        command.error = ParseError{"limit needs to say which colours to keep, "
                                   "as in -limit spread:16 or -limit sort:16", value};
        return false;
    }

    const auto named = Haio::limitNamed(std::string_view{value}.substr(0, colon));
    if (!named) {
        command.error = ParseError{"limit takes sort or spread before its colon", value};
        return false;
    }

    const auto most = countOf(std::string(std::string_view{value}.substr(colon + 1)));
    if (!most) {
        command.error = ParseError{"limit takes a number of colours, counted from one", value};
        return false;
    }
    if (command.pendingLimit) {
        command.error = ParseError{"this limit replaces one nothing has used yet",
                                   command.pendingLimit->value};
        return false;
    }

    command.pendingLimitHow = *named;
    command.pendingLimit = Command::Pending{std::move(option), std::move(value)};
    return true;
}

/** an operation: it takes the settings waiting for it, and needs a filter */
bool addPalette(Command& command, std::string value) {
    if (!command.pendingFilter) {
        command.error = ParseError{"putting a picture into a palette needs a filter first, "
                                   "as in -filter bayer before -palete", value};
        return false;
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

    return addToken(command, Token{.type = TokenType::FilterPalette, .value = std::move(value),
                                   .format = Format::RAW, .dither = *how,
                                   .limit = most, .limitHow = limitHow});
}

/**
 * -crop is the only option that may be written with nothing after it, so it is the
 * only one that has to look before it takes.
 *
 * what it looks for is a geometry, because the word after it is just as likely to be
 * the output path: "convert in.png -crop out.ppm" is a crop of everything, and
 * "convert in.png -crop 10x out.ppm" is a geometry somebody got wrong. the two are
 * told apart by whether anything else follows.
 */
bool readCrop(Command& command, const Lexer::Word& word, size_t& i, std::span<const std::string> args) {
    std::string value{word.value};

    if (!word.hasValue && i + 1 < args.size() && !isOption(args[i + 1])) {
        Rect ignored;
        const std::string_view next = args[i + 1];
        if (parseCropGeometryToken(next, ignored) || parseRectToken(next, ignored)) {
            value = args[++i];
        } else if (i + 2 < args.size()) {
            setError(command, "invalid crop geometry", std::string(next));
            return false;
        }
    }
    return addCrop(command, std::move(value), false);
}

/**
 * an option, read by the table and then handed to whoever knows what it means.
 *
 * everything above the switch is the same for all of them, which is the point of
 * writing the count down: the parser no longer has to be told, once per option, that
 * a value may arrive after a space or after an equals sign.
 */
bool readOption(Command& command, const Lexer::Word& word, size_t& i,
                std::span<const std::string> args, std::string& pendingSize) {
    /**
     * -size was written for the generator that has to come next, so an option
     * arriving first is the mistake -- not whatever that option then turns out to be
     * wrong about, which is a confusing thing to be told instead.
     */
    if (!pendingSize.empty() && word.option->kind != Lexer::Opt::Size) {
        setError(command, "-size is the size a generator is created at; use -resize to scale a picture",
                 pendingSize);
        return false;
    }

    if (word.option->kind == Lexer::Opt::Crop) return readCrop(command, word, i, args);

    const auto value = valueOf(command, word, i, args);
    if (!value) return false;

    const auto option = std::string(word.spelling);
    switch (word.option->kind) {
        case Lexer::Opt::Size:     pendingSize = *value; return true;
        case Lexer::Opt::CropRect: return addCrop(command, *value, true);
        case Lexer::Opt::Resize:   return addResize(command, *value, option);
        case Lexer::Opt::Radius:   return addRadius(command, *value);
        case Lexer::Opt::Format:   return addFormat(command, *value);
        case Lexer::Opt::PixFmt:   return addPixelFormat(command, *value);
        case Lexer::Opt::Filter:   return addFilter(command, *value, option);
        case Lexer::Opt::Limit:    return addLimit(command, *value, option);
        case Lexer::Opt::Palette:  return addPalette(command, *value);
        case Lexer::Opt::Fx:       return addFx(command, *value);
        // taken before the value was read, because its value is optional
        case Lexer::Opt::Crop:     break;
    }
    return true;
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

        if (const auto word = Lexer::optionOf(token)) {
            if (!readOption(command, *word, i, args, pendingSize)) return command;
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

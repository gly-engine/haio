#include <haio_cli.hpp>

#include <boost/spirit/home/x3.hpp>

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace x3 = boost::spirit::x3;

/**
 * how the command line spells the language: words cut the way a shell cuts them,
 * options with a dash, parentheses as themselves, and the output last. this is the
 * only part of parsing that is the command line's own; what the words mean is
 * Haio::Parser's, so the cdn can say the same things in its own spelling.
 */
namespace Haio::Cli {
namespace {

/** the shell's quoting, for a line that arrives as one string; see Grammar::lexerRules */
namespace Words {
const auto paren = x3::string("\\(") | x3::string("\\)") | x3::string("(") | x3::string(")");
const auto singleQuoted = x3::lexeme['\'' >> *(x3::char_ - '\'') >> '\''];
const auto doubleQuoted = x3::lexeme['"' >> *(('\\' >> x3::char_) | (x3::char_ - '"')) >> '"'];
const auto word = x3::lexeme[+(x3::char_ - x3::space - '"' - '\'' - '(' - ')')];
const auto arg = x3::raw[singleQuoted | doubleQuoted | paren | word];
const auto cmdline = x3::skip(x3::space)[*arg];
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

/**
 * "-resize", "--resize=8x8", "-filter": the dashes go, the equals sign splits off.
 *
 * "--name" is "-name" on purpose. imagemagick has one dash and ffmpeg's habits have
 * two, and making somebody remember which of the two haio wanted is not a feature.
 */
Parser::Word named(std::string_view word) {
    const auto equals = word.find('=');
    const auto head = word.substr(0, equals);

    Parser::Word out{Parser::Word::Kind::Named, std::string(word), std::string(head),
                     std::string(head.substr(head.starts_with("--") ? 2 : 1)), std::nullopt};
    if (equals != std::string_view::npos) out.value = std::string(word.substr(equals + 1));
    return out;
}

}

/**
 * the words after "convert", each told what it is: a parenthesis, something named with
 * a dash, the last word as the output the way it is for imagemagick, and everything
 * else a source. "-" alone is stdin or stdout, never an option.
 */
std::vector<Parser::Word> wordsOf(std::span<const std::string> args) {
    using Kind = Parser::Word::Kind;

    std::vector<Parser::Word> out;
    for (size_t at = 1; at < args.size(); at++) {
        const std::string_view word = args[at];
        if (word == "(") {
            out.push_back({Kind::Open, std::string(word)});
        } else if (word == ")") {
            out.push_back({Kind::Close, std::string(word)});
        } else if (word.starts_with('-') && word != "-") {
            out.push_back(named(word));
        } else {
            out.push_back({at + 1 == args.size() ? Kind::Output : Kind::Plain, std::string(word)});
        }
    }
    return out;
}

std::vector<std::string> lexCommandLine(std::string_view text) {
    std::vector<std::string> args;
    auto first = text.begin();
    const auto last = text.end();
    if (!x3::parse(first, last, Words::cmdline, args) || first != last) {
        throw std::runtime_error("unable to parse the command line " + Stages::quoted(std::string(first, last)));
    }

    std::ranges::transform(args, args.begin(), normalizeLexeme);
    return args;
}

Command parseCommandLine(std::string_view text) {
    return Parser::parse(wordsOf(lexCommandLine(text)));
}

Command parseArgs(int argc, char* argv[]) {
    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(std::max(argc, 0)));
    for (int i = 0; i < argc; i++) {
        args.emplace_back(argv[i]);
    }
    return Parser::parse(wordsOf(args));
}

Pipeline buildPipeline(const Command& command) {
    auto pipeline = Parser::build(command);
    if (!pipeline) throw std::runtime_error(pipeline.error().message);
    return *std::move(pipeline);
}

}

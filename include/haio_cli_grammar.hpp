#pragma once

#include "haio_codec.hpp"
#include "haio_formats.hpp"

#include <array>
#include <boost/spirit/home/x3.hpp>
#include <optional>
#include <string>
#include <string_view>

namespace Haio::Cli::Lexer {

namespace x3 = boost::spirit::x3;

inline const auto paren = x3::string("\\(") | x3::string("\\)") | x3::string("(") | x3::string(")");
inline const auto singleQuoted = x3::lexeme['\'' >> *(x3::char_ - '\'') >> '\''];
inline const auto doubleQuoted = x3::lexeme['"' >> *(('\\' >> x3::char_) | (x3::char_ - '"')) >> '"'];
inline const auto word = x3::lexeme[+(x3::char_ - x3::space - '"' - '\'' - '(' - ')')];
inline const auto arg = x3::raw[singleQuoted | doubleQuoted | paren | word];
inline const auto cmdline = x3::skip(x3::space)[*arg];

struct Rule {
    std::string_view name;
    std::string_view expression;
};

struct Note {
    std::string_view text;
};

inline constexpr std::array ebnfRules = {
    Rule{"paren", R"ebnf("\(" | "\)" | "(" | ")")ebnf"},
    Rule{"single-quoted", R"("'" , { any-character - "'" } , "'")"},
    Rule{"double-quoted", R"ebnf(""" , { "\" , any-character | any-character - """ } , """)ebnf"},
    Rule{"word", R"(non-empty-token-without-space-quote-or-paren)"},
    Rule{"arg", "single-quoted | double-quoted | paren | word"},
    Rule{"cmdline", "{ arg }"},
};

/**
 * which option a word turned out to name, so that a spelling is recognised once, by
 * the table below, rather than by whichever branch of the parser gets to it first.
 */
enum class Opt {
    Size,
    Crop,
    CropRect,
    Resize,
    Radius,
    Format,
    PixFmt,
    Filter,
    Limit,
    Palette,
    Fx,
};

/**
 * every option the convert line takes, and how many words follow each.
 *
 * it is a table because the same facts were written down three times: as an if in the
 * parser, as a rule in the grammar this file documents, and as a line in the usage
 * text. three copies of one list is three chances to disagree, and they did --
 * "--size" was in the grammar and in the usage and in neither branch of the parser,
 * and "--palette=cga" went looking for its value in the next word along, because the
 * equals sign was only ever matched against the spelling the error message used.
 *
 * the count is the part that matters. reading an option is the same work whatever it
 * means: take the words the table says it takes, from after an equals sign or from
 * one along, and hand them to whoever knows what they are for. that last step is the
 * only thing particular to the option, and it is all the parser's switch is left
 * doing.
 */
struct Option {
    Opt kind;

    /** every spelling it answers to; the first is the one the usage text shows */
    std::array<std::string_view, 4> spellings;

    /** how many words follow it */
    int args = 1;

    /** ... and whether it can do without them, which only -crop can */
    bool optional = false;

    /**
     * whether it is one of the filters, which is what the grammar calls the options
     * that may repeat between the source and the output. -size is not one: it belongs
     * to the generator that has to follow it.
     */
    bool filter = true;

    /** the name this option's rule has in the grammar */
    std::string_view rule;

    /** and the rule the words after it have to match */
    std::string_view takes;

    std::string_view help;
};

inline constexpr Option options[] = {
    {.kind = Opt::Size, .spellings = {"-size"}, .filter = false,
     .rule = "size-option", .takes = "size",
     .help = "the size a generator is created at"},

    {.kind = Opt::Crop, .spellings = {"-crop"}, .optional = true,
     .rule = "crop-filter", .takes = "crop-geometry",
     .help = "crop, written the way imagemagick writes it"},

    {.kind = Opt::CropRect, .spellings = {"--crop"},
     .rule = "crop-rect-filter", .takes = "rect",
     .help = "crop by an explicit rectangle"},

    {.kind = Opt::Resize, .spellings = {"-resize", "--resize", "--size"},
     .rule = "resize-filter", .takes = "size",
     .help = "resize, to a size or to a share such as 30pct"},

    {.kind = Opt::Radius, .spellings = {"-radius", "--radius"},
     .rule = "radius-filter", .takes = "integer",
     .help = "round the corners away"},

    {.kind = Opt::Format, .spellings = {"-format", "--format"},
     .rule = "format-filter", .takes = "format-name",
     .help = "the container to write, whatever the output is called"},

    {.kind = Opt::PixFmt, .spellings = {"-pix_fmt", "--pix_fmt", "-pix_format", "--pix_format"},
     .rule = "pix-fmt-filter", .takes = "colour-name",
     .help = "the colour stored inside it, as ffmpeg names it"},

    {.kind = Opt::Filter, .spellings = {"-filter", "--filter"},
     .rule = "filter-setting", .takes = "dither-name",
     .help = "how a colour that is not in the palette becomes one that is"},

    {.kind = Opt::Limit, .spellings = {"-limit", "--limit"},
     .rule = "limit-setting", .takes = "limit-spec",
     .help = "keep only some of the palette's colours, and say which"},

    {.kind = Opt::Palette, .spellings = {"-palete", "--palete", "-palette", "--palette"},
     .rule = "palette-filter", .takes = "palette-spec",
     .help = "fit the picture into a named palette"},

    {.kind = Opt::Fx, .spellings = {"-fx"},
     .rule = "fx-filter", .takes = "expression",
     .help = "an expression, read and not yet run"},
};

/** an option, the spelling it arrived as, and whatever an equals sign carried with it */
struct Word {
    const Option* option = nullptr;
    std::string_view spelling;
    std::string_view value;
    bool hasValue = false;
};

/**
 * the option a word names, or nothing when it names none.
 *
 * "-resize=8x8" and "-resize 8x8" are one thing said two ways, so the equals sign is
 * split off here rather than inside each option's own branch: whoever matched a
 * spelling by hand had to remember this too, and one of them did not.
 */
constexpr std::optional<Word> optionOf(std::string_view token) {
    const auto equals = token.find('=');
    const auto head = token.substr(0, equals);

    for (const auto& option : options) {
        for (const auto spelling : option.spellings) {
            if (spelling.empty()) break;
            if (spelling != head) continue;
            return Word{&option, spelling,
                        equals == std::string_view::npos ? std::string_view{} : token.substr(equals + 1),
                        equals != std::string_view::npos};
        }
    }
    return std::nullopt;
}

/** the generators, which are a prefix on the source rather than an option of their own */
inline constexpr std::string_view generatorKinds[] = {"xc", "canvas", "gradient", "radial-gradient"};

namespace Detail {

/** an alternation, broken across lines wherever one would run too wide to read */
consteval void alternative(std::string& out, size_t& lineStart, std::string_view piece) {
    constexpr size_t width = 58;
    if (!out.empty()) {
        if (out.size() - lineStart + piece.size() + 3 > width) {
            out += "\n                  | ";
            lineStart = out.size();
        } else {
            out += " | ";
        }
    }
    out += piece;
}

consteval std::string_view quotedList(auto&& spellings) {
    std::string out;
    size_t lineStart = 0;
    for (const auto spelling : spellings) {
        if (spelling.empty()) break;
        alternative(out, lineStart, "\"" + std::string(spelling) + "\"");
    }
    return std::define_static_string(out);
}

/** '( "-resize" | "--resize" | "--size" ) , size', which is an option's whole grammar */
consteval std::string_view expressionOf(const Option& option) {
    size_t count = 0;
    for (const auto spelling : option.spellings) {
        if (!spelling.empty()) count++;
    }

    std::string out{quotedList(option.spellings)};
    if (count > 1) out = "( " + out + " )";

    if (option.args > 0) {
        out += option.optional ? " , [ " + std::string(option.takes) + " ]"
                               : " , " + std::string(option.takes);
    }
    return std::define_static_string(out);
}

/** one rule per option, off the table rather than beside it */
consteval auto optionRules() {
    std::array<Rule, std::size(options)> out{};
    for (size_t i = 0; i < std::size(options); i++) {
        out[i] = Rule{options[i].rule, expressionOf(options[i])};
    }
    return out;
}

/** the filters, named rather than spelled out, which is what { filter } stands for */
consteval std::string_view filterAlternation() {
    std::string out;
    size_t lineStart = 0;
    for (const auto& option : options) {
        if (option.filter) alternative(out, lineStart, option.rule);
    }
    return std::define_static_string(out);
}

/**
 * every name the Format enum answers to, straight off the enum.
 *
 * the list used to be written out by hand, which is how the grammar came to offer
 * "etc1" and "rgb565" as containers long after they had stopped being any such thing.
 * the parser has always asked the enum; now the page that documents it does too.
 */
consteval std::string_view formatAlternation() {
    std::string out;
    size_t lineStart = 0;
    HAIO_FOR_EACH_FORMAT(e) {
        alternative(out, lineStart, "\"" + std::string(lowerOf(e)) + "\"");
    }
    return std::define_static_string(out);
}

/** and every name a colour answers to, which is the enum plus the borrowed spellings */
consteval std::string_view colorAlternation() {
    std::string out;
    size_t lineStart = 0;
    HAIO_FOR_EACH_COLOR(e) {
        alternative(out, lineStart, "\"" + std::string(lowerOf(e)) + "\"");
    }
    for (const auto& alias : colorAliases) {
        alternative(out, lineStart, "\"" + std::string(alias.spelling) + "\"");
    }
    return std::define_static_string(out);
}

template <size_t A, size_t B, size_t C>
consteval auto joined(const std::array<Rule, A>& a, const std::array<Rule, B>& b, const std::array<Rule, C>& c) {
    std::array<Rule, A + B + C> out{};
    size_t at = 0;
    for (const auto& rule : a) out[at++] = rule;
    for (const auto& rule : b) out[at++] = rule;
    for (const auto& rule : c) out[at++] = rule;
    return out;
}

/** the shape of the line, which is the part no table can say */
inline constexpr std::array structureRules = {
    Rule{"convert-command", R"("convert" , source , { filter } , output)"},
    Rule{"source", "file-spec | generator"},
    Rule{"output", "file-spec"},
    Rule{"file-spec", R"([ format-name , ":" ] , path)"},
    Rule{"path", "arg"},
    Rule{"generator", "size-option , generator-source"},
    Rule{"generator-source", R"(generator-kind , ":" , generator-value)"},
    Rule{"generator-kind", quotedList(generatorKinds)},
    Rule{"filter", filterAlternation()},
};

/** and the words the options take, which is the part no table can say either */
inline constexpr std::array leafRules = {
    Rule{"format-name", formatAlternation()},
    Rule{"colour-name", colorAlternation()},
    Rule{"dither-name", R"("nearest" | "bayer" | "floyd" | "strict")"},
    Rule{"limit-spec", R"(( "sort" | "spread" ) , ":" , integer)"},
    Rule{"palette-spec", R"(palette-name , [ ":" , range , { "," , range } ])"},
    Rule{"range", R"(integer , [ ".." , integer ])"},
    Rule{"size", R"(integer , ( "x" | "X" ) , integer | integer , ( "%" | "pct" ))"},
    Rule{"crop-geometry", R"(integer , ( "x" | "X" ) , integer , [ signed-integer , signed-integer ])"},
    Rule{"rect", R"(integer , "," , integer , "," , integer , "," , integer)"},
    Rule{"integer", "digit , { digit }"},
    Rule{"signed-integer", R"(( "+" | "-" ) , integer)"},
};

}

inline constexpr auto commandRules =
    Detail::joined(Detail::structureRules, Detail::optionRules(), Detail::leafRules);

inline constexpr std::array notes = {
    Note{"`png:-` and `ppm:-` use stdin/stdout with an explicit format."},
    Note{"an unknown prefix such as `foo:bar.png` is parsed as a normal path."},
    Note{"every option takes its words after a space or after an `=`; the two are the same thing."},
    Note{"`-size` is valid only when followed by a generator source, so an option written between the two is an error."},
    Note{"generator sources are parsed but currently rejected during pipeline construction."},
    Note{"`-fx` expressions are parsed but currently rejected during pipeline construction."},
    Note{"`-crop` is the one option whose argument is optional; without geometry it creates a default crop token, which the current c++ pipeline rejects until a default crop operation is defined."},
    Note{"`-filter` and `-limit` are settings: they wait for the operation that uses them, and a line that ends with one still waiting is an error."},
    Note{"`-pix_fmt` names the colour stored inside the output, the way ffmpeg's option does; the container still comes from the output path or `-format`."},
    Note{"naming a colour a container cannot store is an error rather than a silent conversion."},
    Note{"`raw:out.bin` with a `-pix_fmt` writes the pixels themselves, which is the only way to ask for a colour with no container around it."},
    Note{"the format and colour names above are generated from the enums, so they are whatever this build can actually do."},
};

}

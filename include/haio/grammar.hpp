#pragma once

// every stage header and every codec's settings, found by cmake rather than listed here
#include <haio/generated/codec.hpp>
#include <haio/generated/transform.hpp>

#include "haio/stage.hpp"

#include <algorithm>
#include <array>
#include <meta>
#include <string_view>
#include <vector>

/**
 * the language every front end speaks, as data: the stages it can name and the
 * grammar that documents them. the command line spells it with dashes and
 * parentheses, and the cdn will spell the same thing as a query; how the words are
 * cut up is each front end's, and what they mean is here and in include/haio/parser.hpp.
 *
 * there is no list of options here and no list of formats. a stage says which options
 * it reads, and a format, a colour or a palette is a name that is checked against
 * what this build can do when the line runs, not when it is documented.
 */
namespace Haio::Grammar {

struct Rule {
    std::string_view name;
    std::string_view expression;
};

struct Note {
    std::string_view text;
};

namespace Detail {

/** every stage anybody declared, which is every Stage in Haio::Stages that is included */
consteval auto stageMembers() {
    std::vector<std::meta::info> out;
    for (const auto member : std::meta::members_of(^^Haio::Stages, std::meta::access_context::unchecked())) {
        if (std::meta::is_variable(member) && std::meta::type_of(member) == ^^const Stages::Stage) {
            out.push_back(member);
        }
    }
    return std::define_static_array(out);
}

}

inline constexpr auto stages = [] {
    std::array<const Stages::Stage*, Detail::stageMembers().size()> out{};
    size_t at = 0;
    template for (constexpr auto member : Detail::stageMembers()) out[at++] = &[:member:];
    // by name, since the order headers happen to be included in is not an order
    std::ranges::sort(out, {}, &Stages::Stage::rule);
    return out;
}();

/** the stage a word names, when it names one of that kind */
constexpr const Stages::Stage* stageNamed(Stages::Kind kind, std::string_view spelled) {
    for (const auto* stage : stages) {
        if (stage->kind != kind) continue;
        for (const auto spelling : stage->spellings) {
            if (!spelling.empty() && spelling == spelled) return stage;
        }
    }
    return nullptr;
}

/**
 * a codec's own setting, spelled as imagemagick spells it: "-define png:compression-level=9".
 * the key is whatever the codec declared, so there is no list of them here either.
 */
inline constexpr Stages::Option define{
    .spellings = {"define"}, .takes = "define-spec", .repeats = true,
    .help = "a setting of the codec, as png:compression-level=9",
};

/**
 * a path, a url, "-", or a canvas such as xc:white. it takes -define, and whatever
 * the codec of its format declared for reading.
 */
inline constexpr Stages::Stage input{
    .kind = Stages::Kind::Input,
    .spellings = {"input"},
    .rule = "input",
    .takes = "file-spec",
    .help = "a path, a url, a path inside an archive, - for stdin, or a canvas such as xc:white",
};

/** named on its own, since the output looks for it before the rest are taken */
inline constexpr Stages::Option outputFormat{
    .spellings = {"format"}, .takes = "format-name",
    .help = "the container to write, whatever the output is called",
};

inline constexpr Stages::Option outputOptions[] = {
    outputFormat,
};

inline constexpr Stages::Stage output{
    .kind = Stages::Kind::Output,
    .spellings = {"output"},
    .rule = "output",
    .takes = "file-spec",
    .options = outputOptions,
    .help = "a path, or - for stdout",
};

/** a codec that reads settings, and the name its format goes by */
struct CodecReads {
    std::string_view format;
    const Codecs::Reads* reads = nullptr;
};

/** a brush, the name it is written as, and what it takes */
struct BrushDraws {
    Brush brush = Brush::Xc;
    std::string_view name;
    const Codecs::Draws* draws = nullptr;
};

namespace Detail {

template <Format F>
constexpr bool readsAnything = !Codecs::reads<F>.decode.empty() || !Codecs::reads<F>.encode.empty();

consteval size_t readingCodecs() {
    size_t count = 0;
    HAIO_FOR_EACH_FORMAT(e) {
        if (readsAnything<std::meta::extract<Format>(e)>) count++;
    }
    return count;
}

consteval size_t brushCount() {
    return std::meta::enumerators_of(^^Brush).size();
}

}

/** every codec that reads a setting, straight off the Format enum */
inline constexpr auto codecs = [] {
    std::array<CodecReads, Detail::readingCodecs()> out{};
    size_t at = 0;
    HAIO_FOR_EACH_FORMAT(e) {
        constexpr Format format = std::meta::extract<Format>(e);
        if constexpr (Detail::readsAnything<format>) out[at++] = CodecReads{lowerOf(e), &Codecs::reads<format>};
    }
    return out;
}();

/** every brush, straight off the Brush enum, whatever it takes */
inline constexpr auto brushes = [] {
    std::array<BrushDraws, Detail::brushCount()> out{};
    size_t at = 0;
    HAIO_FOR_EACH_BRUSH(e) {
        constexpr Brush brush = std::meta::extract<Brush>(e);
        out[at++] = BrushDraws{brush, kebabOf(e), &Codecs::draws<brush>};
    }
    return out;
}();

/** the options a brush named at runtime takes */
constexpr std::span<const Stages::Option> optionsOf(Brush brush) {
    for (const auto& one : brushes) {
        if (one.brush == brush) return one.draws->options;
    }
    return {};
}

/** how the shell's words become the parser's, which boost.spirit x3 does */
inline constexpr std::array lexerRules = {
    Rule{"paren", R"ebnf("\(" | "\)" | "(" | ")")ebnf"},
    Rule{"single-quoted", R"("'" , { any-character - "'" } , "'")"},
    Rule{"double-quoted", R"ebnf(""" , { "\" , any-character | any-character - """ } , """)ebnf"},
    Rule{"word", R"(non-empty-token-without-space-quote-or-paren)"},
    Rule{"arg", "single-quoted | double-quoted | paren | word"},
    Rule{"cmdline", "{ arg }"},
};

/** the shape of the line, which is the part no stage can say */
inline constexpr std::array structureRules = {
    Rule{"convert-command", R"("convert" , step , { step } , output)"},
    Rule{"step", R"ebnf(source | transform | merge | "(" , step , { step } , ")")ebnf"},
    Rule{"file-spec", R"([ format-name , ":" ] , ( url | path ))"},
    Rule{"url", R"(( "http://" | "https://" ) , arg)"},
    Rule{"path", "arg"},
};

/** and the words the stages take */
inline constexpr std::array leafRules = {
    Rule{"name", R"(letter , { letter | digit | "_" | "-" })"},
    Rule{"format-name", "name"},
    Rule{"colour-name", "name"},
    Rule{"palette-name", "name"},
    Rule{"codec-option", R"("-" , name , arg)"},
    Rule{"define-spec", R"(name , ":" , name , "=" , arg)"},
    Rule{"expression", "arg"},
    Rule{"limit-spec", R"(name , ":" , integer)"},
    Rule{"palette-spec", R"(palette-name , [ ":" , range , { "," , range } ])"},
    Rule{"range", R"(integer , [ ".." , integer ])"},
    Rule{"size", R"(integer , ( "x" | "X" ) , integer | integer , ( "%" | "pct" ))"},
    Rule{"crop-geometry", R"(integer , ( "x" | "X" ) , integer , [ signed-integer , signed-integer ] | rect)"},
    Rule{"rect", R"(integer , "," , integer , "," , integer , "," , integer)"},
    Rule{"offset", "signed-integer , signed-integer"},
    Rule{"integer", "digit , { digit }"},
    Rule{"signed-integer", R"(( "+" | "-" ) , integer)"},
};

inline constexpr std::array notes = {
    Note{"an option is any `-name value` that is not a transform. it waits for the next stage, which takes the ones it knows; one it does not know is an error, and so is one that nothing is left to take."},
    Note{"an option belongs to the stage right after it, so `-filter bayer -palette cga` works and `-palette cga -filter bayer` does not."},
    Note{"a required option is one the stage refuses to run without."},
    Note{"every option and every transform takes its value after a space or after an `=`, and `--name` is the same as `-name`."},
    Note{"`format-name`, `colour-name`, `palette-name` and the names an option takes are checked when the line runs, against what this build can do."},
    Note{"`png:-` and `ppm:-` use stdin/stdout with an explicit format."},
    Note{"an unknown prefix such as `foo:bar.png` is parsed as a normal path."},
    Note{"the input and the output also take the options their codec declares, and `-define` for the ones spelled with a colon; the table above lists them."},
    Note{"a canvas such as `xc:white` is painted by a brush rather than read by a codec, so `-size` is one of its options; `canvas:` is `xc:` and `fractal:` is `plasma:` under another name, as they are for imagemagick."},
    Note{"`gradient:` and `radial-gradient:` draw the same pixels imagemagick 6 does, `-define gradient:*` included, except where imagemagick 6 slips: south by -define measured against the width, and a start pixel in the last column taking its neighbour's colour. `plasma:` is haio's own noise, so a `-seed` repeats haio's picture rather than imagemagick's."},
    Note{"every source puts a picture on a stack, the way imagemagick keeps a list: a file, an http url, `-` for stdin, or a canvas such as `xc:white`. the last word is always the output."},
    Note{"a transform changes the picture on top of the stack, the one made last; imagemagick would change them all. a merge such as `-composite` wants two to four in its parenthesis and leaves one: with three the third tints the second, and with four the fourth tints the second's negative under it."},
    Note{"a parenthesis only scopes the stack: what is inside sees only the pictures made inside, and they join the stack around it at the `)`. options do not cross it."},
    Note{"the output takes exactly one picture, so two left on the stack is an error rather than two files."},
    Note{"`-fx` is parsed but currently rejected during pipeline construction."},
    Note{"naming a colour a container cannot store is an error rather than a silent conversion."},
    Note{"`-pix_fmt` moves the picture on top into that colour, and written right before the output it is also the colour stored inside it."},
    Note{"`raw:out.bin` with a `-pix_fmt` writes the pixels themselves, which is the only way to ask for a colour with no container around it."},
};

}

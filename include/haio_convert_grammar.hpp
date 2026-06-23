#pragma once

#include <array>
#include <boost/spirit/home/x3.hpp>
#include <string_view>

namespace Haio::Convert::Lexer {

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

inline constexpr std::array commandRules = {
    Rule{"convert-command", R"("convert" , source , { filter } , output)"},
    Rule{"source", "file-spec | generator"},
    Rule{"output", "file-spec"},
    Rule{"file-spec", R"([ format-prefix , ":" ] , path)"},
    Rule{"format-prefix", R"("png" | "ppm" | "etc1" | "rgb565" | "pvr" | "dds" | "ktx" | "ktx2" | "raw")"},
    Rule{"path", "arg"},
    Rule{"generator", "size-option , generator-source"},
    Rule{"size-option", R"("-size" , size)"},
    Rule{"generator-source", R"(generator-kind , ":" , generator-value)"},
    Rule{"generator-kind", R"("xc" | "canvas" | "gradient" | "radial-gradient")"},
    Rule{"filter", "crop-filter | resize-filter | radius-filter | format-filter | fx-filter"},
    Rule{"crop-filter", R"("-crop" , [ crop-geometry ]
                  | "--crop" , rect)"},
    Rule{"resize-filter", R"(( "--size" | "--resize" | "-resize" ) , size)"},
    Rule{"radius-filter", R"(( "--radius" | "-radius" ) , integer)"},
    Rule{"format-filter", R"(( "--format" | "-format" ) , format-prefix)"},
    Rule{"fx-filter", R"("-fx" , expression)"},
    Rule{"size", R"(integer , ( "x" | "X" ) , integer)"},
    Rule{"crop-geometry", R"(integer , ( "x" | "X" ) , integer , [ signed-integer , signed-integer ])"},
    Rule{"rect", R"(integer , "," , integer , "," , integer , "," , integer)"},
    Rule{"integer", "digit , { digit }"},
    Rule{"signed-integer", R"(( "+" | "-" ) , integer)"},
};

inline constexpr std::array notes = {
    Note{"`png:-` and `ppm:-` use stdin/stdout with an explicit format."},
    Note{"an unknown prefix such as `foo:bar.png` is parsed as a normal path."},
    Note{"`-size` is valid only when followed by a generator source."},
    Note{"generator sources are parsed but currently rejected during pipeline construction."},
    Note{"`-fx` expressions are parsed but currently rejected during pipeline construction."},
    Note{"`-crop` without geometry creates a default crop token; the current c++ pipeline rejects it until a default crop operation is defined."},
    Note{"`-crop wxh+x+y` and `--crop x,y,w,h` build rectangle crop steps."},
};

}

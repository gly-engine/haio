#include <array>
#include <iostream>
#include <string_view>

struct Rule {
    std::string_view name;
    std::string_view expression;
};

struct Note {
    std::string_view text;
};

int main() {
    constexpr auto rules = std::array{
        Rule{"convert-command", R"("convert" , source , { filter } , output)"},
        Rule{"source", "file-spec | generator"},
        Rule{"output", "file-spec"},
        Rule{"file-spec", R"([ format-prefix , ":" ] , path)"},
        Rule{"format-prefix", R"("png" | "ppm" | "etc1" | "rgb565" | "pvr" | "dds" | "ktx" | "ktx2" | "raw")"},
        Rule{"path", "non-empty-token"},
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

    constexpr auto notes = std::array{
        Note{"`png:-` and `ppm:-` use stdin/stdout with an explicit format."},
        Note{"an unknown prefix such as `foo:bar.png` is parsed as a normal path."},
        Note{"`-size` is valid only when followed by a generator source."},
        Note{"generator sources are parsed but currently rejected during pipeline construction."},
        Note{"`-fx` expressions are parsed but currently rejected during pipeline construction."},
        Note{"`-crop` without geometry creates a default crop token; the current c++ pipeline rejects it until a default crop operation is defined."},
        Note{"`-crop wxh+x+y` and `--crop x,y,w,h` build rectangle crop steps."},
    };

    std::cout << "# convert cli grammar\n\n";
    std::cout << "parser implementation: boost.spirit x3.\n\n";
    std::cout << "```ebnf\n";
    for (const auto& rule : rules) {
        std::cout << rule.name;
        for (auto padding = 18 - static_cast<int>(rule.name.size()); padding > 0; --padding) {
            std::cout << ' ';
        }
        std::cout << " = " << rule.expression << " ;\n";
    }
    std::cout << "```\n\n";
    std::cout << "## behavior\n\n";
    for (const auto& note : notes) {
        std::cout << "- " << note.text << '\n';
    }

    return 0;
}

// Prints the convert grammar to stdout, as markdown, for doxygen to pick up: the
// build writes it to build/docs/ebnf.md, a page in the convert topic.
//
// Nothing here is written down twice: the stages come from include/haio/transforms/
// and include/haio/generators/ by reflection, and each one's rule and options come
// from its own declaration.
//
// usage: generate_ebnf > build/docs/ebnf.md

#include <haio/grammar.hpp>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace {

using Haio::Stages::Kind;
using Haio::Stages::Option;
using Haio::Stages::Stage;
namespace Grammar = Haio::Grammar;

struct Line {
    std::string name;
    std::string expression;
};

/**
 * an alternation, broken across lines wherever one would run too wide to read. the
 * break is a bare newline; print() indents it to line up under the "=".
 */
std::string alternation(const std::vector<std::string>& pieces) {
    constexpr size_t width = 58;
    std::string out;
    size_t lineStart = 0;
    for (const auto& piece : pieces) {
        if (!out.empty()) {
            if (out.size() - lineStart + piece.size() + 3 > width) {
                out += "\n| ";
                lineStart = out.size();
            } else {
                out += " | ";
            }
        }
        out += piece;
    }
    return out;
}

/** '( "-palette" | "-palete" )', or the one spelling bare when there is only one */
std::string spelled(const auto& spellings, std::string_view prefix) {
    std::vector<std::string> pieces;
    for (const auto spelling : spellings) {
        if (!spelling.empty()) pieces.push_back("\"" + std::string(prefix) + std::string(spelling) + "\"");
    }
    const auto out = alternation(pieces);
    return pieces.size() > 1 ? "( " + out + " )" : out;
}

std::string optionRule(const Stage& stage) {
    return std::string(stage.rule) + "-option";
}

std::vector<std::string> optionsOf(const Stage& stage) {
    std::vector<std::string> options;
    for (const auto& option : stage.options) {
        options.push_back(spelled(option.spellings, "-") + " , " + std::string(option.takes));
    }
    return options;
}

/** "{ palette-option } , ( "-palette" | "-palete" ) , palette-spec", and its options */
void stageRules(const Stage& stage, std::vector<Line>& out) {
    std::string expression;
    if (!stage.options.empty()) expression = "{ " + optionRule(stage) + " } , ";

    switch (stage.kind) {
        case Kind::Transform:
            expression += spelled(stage.spellings, "-");
            if (!stage.takes.empty()) expression += " , " + std::string(stage.takes);
            break;
        case Kind::Input:
        case Kind::Output:
            expression += std::string(stage.takes);
            break;
    }
    out.push_back({std::string(stage.rule), expression});

    if (stage.options.empty()) return;
    out.push_back({optionRule(stage), alternation(optionsOf(stage))});
}

/** every transform that merges or that does not, named rather than spelled out */
std::string transformAlternation(bool merges) {
    std::vector<std::string> rules;
    for (const auto* stage : Grammar::stages) {
        if (stage->kind == Kind::Transform && stage->merges == merges) rules.emplace_back(stage->rule);
    }
    return alternation(rules);
}

void print(const std::vector<Line>& lines) {
    size_t width = 0;
    for (const auto& line : lines) width = std::max(width, line.name.size());
    for (const auto& line : lines) {
        std::string expression;
        for (const char c : line.expression) {
            expression += c;
            if (c == '\n') expression += std::string(width + 1, ' ');
        }
        std::cout << line.name << std::string(width - line.name.size() + 1, ' ') << "= " << expression << " ;\n";
    }
}

template <typename Rules>
void append(const Rules& rules, std::vector<Line>& out) {
    for (const auto& rule : rules) out.push_back({std::string(rule.name), std::string(rule.expression)});
}

/** which options each stage takes, and which it will not run without */
void printOptions() {
    std::cout << "## options\n\n"
              << "| stage | option | required | what it is for |\n"
              << "|---|---|---|---|\n";

    const auto row = [](std::string_view where, const Option& option) {
        const auto colon = option.spellings[0].find(':') != std::string_view::npos;
        std::cout << "| `" << where << "` | `" << (colon ? "-define " : "-") << option.spellings[0]
                  << (colon ? "=" : " ") << option.takes << "` | " << (option.required ? "yes" : "no") << " | "
                  << Haio::Stages::describe(option) << " |\n";
    };
    for (const auto* stage : Grammar::stages) {
        for (const auto& option : stage->options) row(stage->rule, option);
    }
    for (const auto& option : Grammar::output.options) row("output", option);

    // the codecs' own, which the input or the output takes when it is of that format
    for (const auto& codec : Grammar::codecs) {
        const auto input = std::string(codec.format) + " input";
        const auto output = std::string(codec.format) + " output";
        for (const auto& option : codec.reads->decode) row(input, option);
        for (const auto& option : codec.reads->encode) row(output, option);
    }
    for (const auto& brush : Grammar::brushes) {
        for (const auto& option : brush.draws->options) row(std::string(brush.name) + ":", option);
    }
    std::cout << '\n';
}

}

int main() {
    std::vector<Line> lexer;
    append(Grammar::lexerRules, lexer);

    std::vector<Line> command;
    append(Grammar::structureRules, command);
    command.push_back({"transform", transformAlternation(false)});
    command.push_back({"merge", transformAlternation(true)});
    command.push_back({"source", "{ codec-setting } , ( drawn | file-spec )"});

    // every codec that draws rather than reads, and what it takes after its colon
    std::vector<std::string> drawn;
    for (const auto& brush : Grammar::brushes) {
        auto piece = "\"" + std::string(brush.name) + ":\"";
        if (!brush.draws->takes.empty()) piece += " , [ " + std::string(brush.draws->takes) + " ]";
        drawn.push_back(std::move(piece));
    }
    command.push_back({"drawn", alternation(drawn)});
    command.push_back({"output", "{ output-option | codec-setting } , file-spec"});
    command.push_back({"output-option", alternation(optionsOf(Grammar::output))});
    command.push_back({"codec-setting", "\"-define\" , define-spec | codec-option"});
    for (const auto* stage : Grammar::stages) stageRules(*stage, command);
    append(Grammar::leafRules, command);

    // an explicit page, since a markdown file that only says @ingroup is taken for
    // a file reference rather than a page, and lands in no topic at all
    std::cout << "@page convert_ebnf convert cli grammar\n"
              << "@ingroup convert\n\n"
              << "parser implementation: boost.spirit x3.\n\n"
              << "```ebnf\n";
    print(lexer);
    std::cout << '\n';
    print(command);
    std::cout << "```\n\n";

    printOptions();

    std::cout << "## behavior\n\n";
    for (const auto& note : Grammar::notes) std::cout << "- " << note.text << '\n';
    return 0;
}

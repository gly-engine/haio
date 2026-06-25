#include <haio_convert_grammar.hpp>

#include <algorithm>
#include <iostream>
#include <ranges>

namespace {

template <typename Rules>
void printRules(const Rules& rules) {
    const auto width = std::ranges::max(
        rules | std::views::transform([](const auto& rule) { return rule.name.size(); })
    );
    for (const auto& rule : rules) {
        std::cout << rule.name;
        for (auto padding = static_cast<int>(width - rule.name.size() + 1); padding > 0; --padding) {
            std::cout << ' ';
        }
        std::cout << "= " << rule.expression << " ;\n";
    }
}

}

int main() {
    std::cout << "# convert cli grammar\n\n";
    std::cout << "parser implementation: boost.spirit x3.\n\n";
    std::cout << "```ebnf\n";
    printRules(Haio::Convert::Lexer::ebnfRules);
    std::cout << '\n';
    printRules(Haio::Convert::Lexer::commandRules);
    std::cout << "```\n\n";
    std::cout << "## behavior\n\n";
    for (const auto& note : Haio::Convert::Lexer::notes) {
        std::cout << "- " << note.text << '\n';
    }

    return 0;
}

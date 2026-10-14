#pragma once

#include "haio/implementation/language/parser.hpp"
#include "haio/implementation/language/token.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

/**
 * the convert command line's spelling: the tokenizer that cuts argv into words.
 * what the words mean is Language::Parser's, shared with every other front end.
 */
namespace Haio::Implementation::LanguageParser {

/** argv, "convert" first, as the words Parser reads */
std::vector<Word> wordsOf(std::span<const std::string> args);

Command parseArgs(int argc, char* argv[]);
Command parseCommandLine(std::string_view text);
std::vector<std::string> lexCommandLine(std::string_view text);

/** build, for a caller that would rather have an exception than a Result */
Language::Pipeline buildPipeline(const Command& command);

}

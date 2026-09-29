#pragma once

#include "haio.hpp"
#include "haio/parser.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

/**
 * the convert command line.
 *
 * it used to be namespace Haio::Convert, which now belongs to the colour conversion
 * family -- a name cannot be a namespace and a function template at once.
 *
 * only the spelling is kept here: the tokenizer that cuts argv into words. what the
 * words mean is Haio::Parser's, shared with every other front end.
 */
namespace Haio::Cli {

using ParseError = Parser::Error;
using Input = Parser::Input;
using Command = Parser::Parsed;

/** argv, "convert" first, as the words Haio::Parser reads */
std::vector<Parser::Word> wordsOf(std::span<const std::string> args);

Command parseArgs(int argc, char* argv[]);
Command parseCommandLine(std::string_view text);
std::vector<std::string> lexCommandLine(std::string_view text);

/** Parser::build, for a caller that would rather have an exception than a Result */
Pipeline buildPipeline(const Command& command);

int runCli(int argc, char* argv[]);

}

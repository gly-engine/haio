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

/**
 * @name output escapes
 * imagemagick's percent escapes in the output name, filled in from the picture
 * written: %t the source's name
 * without its folder or extension, %f with its extension, %e the extension alone,
 * %[directory] its folder, %i or %M the source as written, %w and %h the size it is
 * written at, %G the size it was read at, %m what it was read as, %B how many bytes
 * it was, %A whether it has an alpha, %n how many pictures there are, and %p, %s or
 * %d its place among them. %% is a percent sign. every escape takes printf's shape:
 * %-10t, %10B, %010B for a number, %.3t.
 *
 * a source quoted so the shell leaves its pattern alone, as '*.png', or '**.png' for
 * every folder below, is
 * expanded by haio and the whole line runs once per file, so its output has to be
 * named after each, as %t.jpeg. unquoted, the shell spreads the files into the stack
 * of one run instead. only the command line does either; the cdn reads neither.
 *
 * print:'%t\t%G\n' as the output writes no picture: it prints the format, escapes
 * filled in, for every picture, so "'*.png' print:'%t %wx%h\n'" lists them.
 * @{
 */
bool hasEscapes(std::string_view name);

/** whether the name says something different for every file, so a pattern may run the line once per file */
bool namesEach(std::string_view name);

Result<std::string> expandEscapes(std::string_view name, const Written& picture, size_t index, size_t count);

std::string unescapeText(std::string_view text);
/** @} */

int runCli(int argc, char* argv[]);

}

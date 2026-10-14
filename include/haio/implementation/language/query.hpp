#pragma once

#include "haio/implementation/language/token.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

/**
 * the cdn's spelling of a pipeline, a url query: "resize=8x8&format=png". it is the
 * query's own tokenizer, which the command line's argv has in frontend/convert.hpp.
 */
namespace Haio::Implementation::LanguageParser {

std::vector<Language::Token> parseQueryTokens(std::string_view query);
std::unordered_map<std::string, std::string> parseQueryMap(std::string_view query);

}

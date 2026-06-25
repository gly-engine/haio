#include <haio.hpp>
#include <haio_string.hpp>

#include <boost/url/encoding_opts.hpp>
#include <boost/url/parse.hpp>

#include <stdexcept>

namespace Haio {

Pipeline& Pipeline::operator|=(Token token) {
    tokens_.push_back(std::move(token));
    return *this;
}

const std::vector<Token>& Pipeline::tokens() const {
    return tokens_;
}

namespace Tokens {
Token Source(std::string bucket, std::string path) {
    Token token{TokenKind::Source};
    token.bucket = std::move(bucket);
    token.path = std::move(path);
    return token;
}

Token DecodeAuto() {
    return Token{TokenKind::DecodeAuto};
}

Token Decode(Format format) {
    Token token{TokenKind::Decode};
    token.format = format;
    return token;
}

Token Crop(Rect rect) {
    Token token{TokenKind::Crop};
    token.rect = rect;
    return token;
}

Token Resize(Size size) {
    Token token{TokenKind::Resize};
    token.size = size;
    return token;
}

Token Radius(int radius) {
    Token token{TokenKind::Radius};
    token.radius = radius;
    return token;
}

Token Encode(Format format) {
    Token token{TokenKind::Encode};
    token.format = format;
    return token;
}
}

std::unordered_map<std::string, std::string> parseQueryMap(std::string_view query) {
    std::unordered_map<std::string, std::string> out;
    if (!query.empty() && query.front() == '?') query.remove_prefix(1);
    if (query.empty()) return out;

    std::string target = "/?";
    target.append(query);

    auto parsed = boost::urls::parse_origin_form(target);
    if (!parsed) throw std::runtime_error("invalid query: " + parsed.error().message());

    boost::urls::encoding_opts opts;
    opts.space_as_plus = true;
    for (const auto& param : parsed->params(opts)) {
        if (!param.key.empty()) out[param.key] = param.has_value ? param.value : std::string{};
    }
    return out;
}

std::vector<Token> parseQueryTokens(std::string_view query) {
    const auto values = parseQueryMap(query);
    std::vector<Token> tokens;

    if (auto it = values.find("crop"); it != values.end()) tokens.push_back(Tokens::Crop(String::getRect(it->second)));
    if (auto it = values.find("size"); it != values.end()) tokens.push_back(Tokens::Resize(String::getSize(it->second)));
    if (auto it = values.find("resize"); it != values.end()) tokens.push_back(Tokens::Resize(String::getSize(it->second)));
    if (auto it = values.find("radius"); it != values.end()) tokens.push_back(Tokens::Radius(String::getInt(it->second)));
    if (auto it = values.find("format"); it != values.end()) tokens.push_back(Tokens::Encode(formatFromName(it->second)));

    return tokens;
}

}

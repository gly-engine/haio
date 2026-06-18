#include <haio.hpp>

#include <boost/url/encoding_opts.hpp>
#include <boost/url/parse.hpp>

#include <charconv>
#include <stdexcept>

namespace {

int parseInt(std::string_view value) {
    int out = 0;
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto [ptr, ec] = std::from_chars(begin, end, out);
    if (ec != std::errc{} || ptr != end) throw std::runtime_error("invalid integer: " + std::string(value));
    return out;
}

std::vector<std::string_view> split(std::string_view value, char sep) {
    std::vector<std::string_view> parts;
    while (true) {
        const auto pos = value.find(sep);
        parts.push_back(value.substr(0, pos));
        if (pos == std::string_view::npos) break;
        value.remove_prefix(pos + 1);
    }
    return parts;
}

Haio::Size parseSize(std::string_view value) {
    const auto sep = value.find_first_of("xX");
    if (sep == std::string_view::npos) {
        const auto side = parseInt(value);
        return {side, side};
    }
    return {parseInt(value.substr(0, sep)), parseInt(value.substr(sep + 1))};
}

Haio::Rect parseRect(std::string_view value) {
    const auto parts = split(value, ',');
    if (parts.size() != 4) throw std::runtime_error("crop expects x,y,width,height");
    return {parseInt(parts[0]), parseInt(parts[1]), parseInt(parts[2]), parseInt(parts[3])};
}

}

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

    if (auto it = values.find("crop"); it != values.end()) tokens.push_back(Tokens::Crop(parseRect(it->second)));
    if (auto it = values.find("size"); it != values.end()) tokens.push_back(Tokens::Resize(parseSize(it->second)));
    if (auto it = values.find("resize"); it != values.end()) tokens.push_back(Tokens::Resize(parseSize(it->second)));
    if (auto it = values.find("radius"); it != values.end()) tokens.push_back(Tokens::Radius(parseInt(it->second)));
    if (auto it = values.find("format"); it != values.end()) tokens.push_back(Tokens::Encode(formatFromName(it->second)));

    return tokens;
}

}

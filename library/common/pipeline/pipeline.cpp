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

Token ResizeByPercent(int percent) {
    Token token;
    token.kind = TokenKind::Resize;
    token.percent = percent;
    return token;
}

Token Palette(std::string palette, Dither dither, size_t limit, Limit limitHow) {
    Token token;
    token.kind = TokenKind::Palette;
    token.palette = std::move(palette);
    token.dither = dither;
    token.limit = limit;
    token.limitHow = limitHow;
    return token;
}

Token Radius(int radius) {
    Token token{TokenKind::Radius};
    token.radius = radius;
    return token;
}

Token Encode(Format format, std::optional<Color> color) {
    Token token{TokenKind::Encode};
    token.format = format;
    token.color = color;
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
    /**
     * a url cannot carry a percent sign: "%" opens an escape, so "?resize=30%" is a
     * broken request rather than a small picture. "30pct" says the same thing and
     * survives, and both spellings are read here so a query and a command line can
     * be written the same way.
     */
    const auto resizeToken = [](const std::string& value) {
        const auto share = String::getPercent(value);
        return share != 0 ? Tokens::ResizeByPercent(share) : Tokens::Resize(String::getSize(value));
    };

    if (auto it = values.find("size"); it != values.end()) tokens.push_back(resizeToken(it->second));
    if (auto it = values.find("resize"); it != values.end()) tokens.push_back(resizeToken(it->second));
    if (auto it = values.find("radius"); it != values.end()) tokens.push_back(Tokens::Radius(String::getInt(it->second)));
    /**
     * the colour inside the container, spelled the way ffmpeg spells it.
     *
     * it can arrive without a format, and that is a request rather than a mistake:
     * "?pix_fmt=rgb565" with nothing to wrap it is raw pixels in that colour, which
     * is what a caller loading a texture straight into a gpu is asking for.
     */
    auto pixelFormat = values.find("pix_fmt");
    if (pixelFormat == values.end()) pixelFormat = values.find("pix_format");

    std::optional<Color> color;
    if (pixelFormat != values.end()) {
        color = colorNamed(pixelFormat->second);
        if (!color) throw std::runtime_error("unknown pixel format: " + pixelFormat->second);
    }

    if (auto it = values.find("format"); it != values.end()) {
        tokens.push_back(Tokens::Encode(formatFromName(it->second), color));
    } else if (color) {
        tokens.push_back(Tokens::Encode(Format::RAW, color));
    }

    return tokens;
}

}

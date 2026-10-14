#include <haio/implementation/language/token.hpp>

#include <stdexcept>

namespace Haio::Implementation::Language {

Pipeline& Pipeline::operator|=(Token token) {
    tokens_.push_back(std::move(token));
    return *this;
}

const std::vector<Token>& Pipeline::tokens() const {
    return tokens_;
}

}

/** a token for each stage, made the way the stage asks */
namespace Haio::Implementation::LanguageToken {

Language::Token Source(std::string bucket, std::string path) {
    Language::Token token{Language::TokenKind::Source};
    token.bucket = std::move(bucket);
    token.path = std::move(path);
    return token;
}

Language::Token Generate(Codecs::Brush brush, std::string words, Settings settings) {
    Language::Token token{Language::TokenKind::Generate};
    token.brush = brush;
    token.expression = std::move(words);
    token.settings = std::move(settings);
    return token;
}

Language::Token DecodeAuto() {
    return Language::Token{Language::TokenKind::DecodeAuto};
}

Language::Token Decode(Format format, Settings settings) {
    Language::Token token{Language::TokenKind::Decode};
    token.format = format;
    token.settings = std::move(settings);
    return token;
}

Language::Token Crop(Rect rect) {
    Language::Token token{Language::TokenKind::Crop};
    token.rect = rect;
    return token;
}

Language::Token Resize(Size size, Transforms::Transform<>::ResizeFilter filter) {
    Language::Token token{Language::TokenKind::Resize};
    token.size = size;
    token.filter = filter;
    return token;
}

Language::Token ResizeByPercent(int percent, Transforms::Transform<>::ResizeFilter filter) {
    Language::Token token;
    token.kind = Language::TokenKind::Resize;
    token.percent = percent;
    token.filter = filter;
    return token;
}

Language::Token Palette(std::string palette, WIP::Dither dither, size_t limit, WIP::Limit limitHow) {
    Language::Token token;
    token.kind = Language::TokenKind::Palette;
    token.palette = std::move(palette);
    token.dither = dither;
    token.limit = limit;
    token.limitHow = limitHow;
    return token;
}

Language::Token Composite(Transforms::Compose compose, Transforms::Transform<>::Gravity gravity, int x, int y) {
    Language::Token token{Language::TokenKind::Composite};
    token.compose = compose;
    token.gravity = gravity;
    token.rect.x = x;
    token.rect.y = y;
    return token;
}

Language::Token Fx(std::string expression) {
    Language::Token token{Language::TokenKind::Fx};
    token.expression = std::move(expression);
    return token;
}

Language::Token Open() {
    return Language::Token{Language::TokenKind::Open};
}

Language::Token Close() {
    return Language::Token{Language::TokenKind::Close};
}

Language::Token Radius(int radius) {
    Language::Token token{Language::TokenKind::Radius};
    token.radius = radius;
    return token;
}

Language::Token Negate() {
    return Language::Token{Language::TokenKind::Negate};
}

Language::Token PixFmt(Color color) {
    Language::Token token{Language::TokenKind::PixFmt};
    token.color = color;
    return token;
}

Language::Token Encode(Format format, std::optional<Color> color, Settings settings) {
    Language::Token token{Language::TokenKind::Encode};
    token.format = format;
    token.color = color;
    token.settings = std::move(settings);
    return token;
}
}

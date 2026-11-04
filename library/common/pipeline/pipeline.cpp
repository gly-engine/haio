#include <haio.hpp>

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

Token Generate(Brush brush, std::string words, Settings settings) {
    Token token{TokenKind::Generate};
    token.brush = brush;
    token.expression = std::move(words);
    token.settings = std::move(settings);
    return token;
}

Token DecodeAuto() {
    return Token{TokenKind::DecodeAuto};
}

Token Decode(Format format, Settings settings) {
    Token token{TokenKind::Decode};
    token.format = format;
    token.settings = std::move(settings);
    return token;
}

Token Crop(Rect rect) {
    Token token{TokenKind::Crop};
    token.rect = rect;
    return token;
}

Token Resize(Size size, ResizeFilter filter, bool exact) {
    Token token{TokenKind::Resize};
    token.size = size;
    token.filter = filter;
    token.exact = exact;
    return token;
}

Token ResizeByPercent(int percent, ResizeFilter filter) {
    Token token;
    token.kind = TokenKind::Resize;
    token.percent = percent;
    token.filter = filter;
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

Token Composite(Compose compose, Gravity gravity, int x, int y) {
    Token token{TokenKind::Composite};
    token.compose = compose;
    token.gravity = gravity;
    token.rect.x = x;
    token.rect.y = y;
    return token;
}

Token Grid(Size cells, Size quantum, Size gap, Gravity gravity) {
    Token token{TokenKind::Grid};
    token.cells = cells;
    token.quantum = quantum;
    token.gap = gap;
    token.gravity = gravity;
    return token;
}

Token Fx(std::string expression) {
    Token token{TokenKind::Fx};
    token.expression = std::move(expression);
    return token;
}

Token Open() {
    return Token{TokenKind::Open};
}

Token Close() {
    return Token{TokenKind::Close};
}

Token Radius(int radius) {
    Token token{TokenKind::Radius};
    token.radius = radius;
    return token;
}

Token Negate() {
    return Token{TokenKind::Negate};
}

Token PixFmt(Color color) {
    Token token{TokenKind::PixFmt};
    token.color = color;
    return token;
}

Token Encode(Format format, std::optional<Color> color, Settings settings) {
    Token token{TokenKind::Encode};
    token.format = format;
    token.color = color;
    token.settings = std::move(settings);
    return token;
}

Token Deliver(BodyEncoding encoding, std::string name) {
    Token token{TokenKind::Deliver};
    token.encoding = encoding;
    token.name = std::move(name);
    return token;
}
}

}

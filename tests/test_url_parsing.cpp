#include <haio.hpp>

#include <cassert>
#include <stdexcept>

int main() {
    const auto values = Haio::parseQueryMap("?size=16x16&format=webp&name=hello+world&file=a%2Fb.png");
    assert(values.at("size") == "16x16");
    assert(values.at("format") == "webp");
    assert(values.at("name") == "hello world");
    assert(values.at("file") == "a/b.png");

    const auto tokens = Haio::parseQueryTokens("resize=8x4&radius=2");
    assert(tokens.size() == 2);
    assert(tokens[0].kind == Haio::TokenKind::Resize);
    assert(tokens[0].size.width == 8);
    assert(tokens[0].size.height == 4);
    assert(tokens[1].kind == Haio::TokenKind::Radius);
    assert(tokens[1].radius == 2);

    bool failed = false;
    try {
        (void)Haio::parseQueryMap("size=%zz");
    } catch (const std::runtime_error&) {
        failed = true;
    }
    assert(failed);

    return 0;
}

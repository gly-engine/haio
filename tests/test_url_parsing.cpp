#include <haio.hpp>

#include <cassert>
#include <stdexcept>

int main() {
    const auto values = Haio::parseQueryMap("?size=16x16&format=png&name=hello+world&file=a%2Fb.png");
    assert(values.at("size") == "16x16");
    assert(values.at("format") == "png");
    assert(values.at("name") == "hello world");
    assert(values.at("file") == "a/b.png");

    const auto tokens = Haio::parseQueryTokens("resize=8x4&radius=2");
    assert(tokens.size() == 2);
    assert(tokens[0].kind == Haio::TokenKind::Resize);
    assert(tokens[0].size.width == 8);
    assert(tokens[0].size.height == 4);
    assert(tokens[1].kind == Haio::TokenKind::Radius);
    assert(tokens[1].radius == 2);

    // the colour inside the container, which a query names the way a command line does
    {
        const auto asked = Haio::parseQueryTokens("format=tga&pix_fmt=bgr888");
        assert(asked.size() == 1);
        assert(asked[0].kind == Haio::TokenKind::Encode);
        assert(asked[0].format == Haio::Format::TGA);
        assert(asked[0].color == Haio::Color::BGR888);
    }
    {
        // a colour with no container is the pixels themselves, which is a request and
        // not a mistake
        const auto bare = Haio::parseQueryTokens("pix_fmt=yuv420p");
        assert(bare.size() == 1);
        assert(bare[0].format == Haio::Format::RAW);
        assert(bare[0].color == Haio::Color::YUV420);
    }
    {
        const auto plain = Haio::parseQueryTokens("format=png");
        assert(plain.size() == 1);
        assert(!plain[0].color);
    }

    bool failed = false;
    try {
        (void)Haio::parseQueryMap("size=%zz");
    } catch (const std::runtime_error&) {
        failed = true;
    }
    assert(failed);

    // a colour nobody has is worth stopping for rather than quietly becoming rgba8888
    failed = false;
    try {
        (void)Haio::parseQueryTokens("pix_fmt=yuv444p");
    } catch (const std::runtime_error&) {
        failed = true;
    }
    assert(failed);

    return 0;
}

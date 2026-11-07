#include <haio_cli.hpp>
#include <haio/transforms/blur.hpp>

#include <cassert>
#include <string>
#include <vector>

namespace {

Haio::Cli::Command parse(std::vector<std::string> args) {
    std::vector<char*> argv;
    argv.reserve(args.size());
    for (auto& arg : args) argv.push_back(arg.data());
    return Haio::Cli::parseArgs(static_cast<int>(argv.size()), argv.data());
}

/** the box radius -blur VALUE comes out as, or -1 when it is refused */
int radiusOf(const std::string& value) {
    const auto cmd = parse({"convert", "in.png", "-blur", value, "out.png"});
    if (cmd.error) return -1;
    for (const auto& token : cmd.steps) {
        if (token.kind == Haio::TokenKind::Blur) return token.radius;
    }
    assert(false && "no blur token");
    return -1;
}

void testSigmaBecomesTheBoxRadius() {
    assert(radiusOf("0x8") == 8);
    assert(radiusOf("0x20") == 20);
    assert(radiusOf("0x1.5") == 1);
    assert(radiusOf("3x2") == 2);
    assert(radiusOf("0X4") == 4);
    assert(radiusOf("x4") == 4);
    assert(radiusOf("0x0") == 0);
    // a bare radius takes imagemagick's sigma of one
    assert(radiusOf("5") == 1);
}

void testRefusesWhatImagemagickWouldNotRead() {
    for (const auto* bad : {"abc", "-1", "0x-2", "1x", "0x8x2", "x", "0xinf"}) assert(radiusOf(bad) == -1);
}

Haio::Image<Haio::Color::RGBA8888> filled(int width, int height, auto&& colourAt) {
    std::vector<uint8_t> data(static_cast<size_t>(width) * height * 4);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const auto rgba = colourAt(x, y);
            std::copy(rgba.begin(), rgba.end(), data.begin() + (static_cast<std::ptrdiff_t>(y) * width + x) * 4);
        }
    }
    return Haio::Image<Haio::Color::RGBA8888>{width, height, std::move(data)};
}

void testFlatStaysFlatAndZeroDoesNothing() {
    const auto flat = filled(13, 9, [](int, int) { return std::array<uint8_t, 4>{17, 130, 251, 200}; });
    const auto blurred = Haio::Transforms::Blur<Haio::Color::RGBA8888>(flat, 5);
    assert(blurred && blurred->data == flat.data);

    const auto busy = filled(7, 5, [](int x, int y) {
        return std::array<uint8_t, 4>{static_cast<uint8_t>(x * 30), static_cast<uint8_t>(y * 50), 9, 255};
    });
    const auto untouched = Haio::Transforms::Blur<Haio::Color::RGBA8888>(busy, 0);
    assert(untouched && untouched->data == busy.data);
    assert((!Haio::Transforms::Blur<Haio::Color::RGBA8888>(busy, -1)));
}

void testAnEdgeBecomesARampWithoutDarkening() {
    const auto edge = filled(20, 3, [](int x, int) {
        const uint8_t v = x < 10 ? 0 : 255;
        return std::array<uint8_t, 4>{v, v, v, 255};
    });
    const auto blurred = Haio::Transforms::Blur<Haio::Color::RGBA8888>(edge, 3);
    assert(blurred);
    const auto at = [&](int x) { return blurred->data[(static_cast<size_t>(1) * 20 + x) * 4]; };
    for (int x = 1; x < 20; x++) assert(at(x) >= at(x - 1));
    assert(at(0) == 0 && at(19) == 255);
    // symmetric around the edge, which a blur that rounds down would not be
    for (int x = 0; x < 10; x++) assert(at(x) + at(19 - x) == 255);
}

}

int main() {
    testSigmaBecomesTheBoxRadius();
    testRefusesWhatImagemagickWouldNotRead();
    testFlatStaysFlatAndZeroDoesNothing();
    testAnEdgeBecomesARampWithoutDarkening();
    return 0;
}

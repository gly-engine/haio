#include <haio.hpp>

#include <iostream>
#include <string>

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (ok) return;
    std::cerr << "fail: " << what << '\n';
    failures++;
}

}

// the block and planar colours have no stride, so a transform cannot name them
static_assert(Haio::Addressable<Haio::Color::RGBA8888>);
static_assert(Haio::Addressable<Haio::Color::RGB888>);
static_assert(Haio::Addressable<Haio::Color::RGB565>);
static_assert(Haio::Addressable<Haio::Color::GRAY8>);
static_assert(!Haio::Addressable<Haio::Color::ETC1>);
static_assert(!Haio::Addressable<Haio::Color::YUV420>);

// rounding clears an alpha, so only the colours that carry one qualify
static_assert(Haio::Maskable<Haio::Color::RGBA8888>);
static_assert(!Haio::Maskable<Haio::Color::RGB888>);
static_assert(!Haio::Maskable<Haio::Color::GRAY8>);
static_assert(!Haio::Maskable<Haio::Color::ETC1>);
static_assert(Haio::alphaOffsetOf(Haio::Color::RGBA8888) == 3);
static_assert(Haio::alphaOffsetOf(Haio::Color::RGB888) < 0);

// the stride is the whole rule, and sizeOf reads the same one
static_assert(Haio::strideOf(Haio::Color::RGBA8888) == 4);
static_assert(Haio::strideOf(Haio::Color::GRAY8) == 1);
static_assert(Haio::strideOf(Haio::Color::ETC1) == 0);
static_assert(Haio::strideOf(Haio::Color::YUV420) == 0);

// what the concepts buy: a call that the old signature could not express
template <typename T>
concept CanCrop = requires (T image) { Haio::cropImage(image, Haio::Rect{0, 0, 1, 1}); };
template <typename T>
concept CanRound = requires (T image) { Haio::roundImageCorners(image, 1); };

static_assert(CanCrop<Haio::Image<Haio::Color::RGB565>>);
static_assert(!CanCrop<Haio::Image<Haio::Color::ETC1>>);
static_assert(CanRound<Haio::Image<Haio::Color::RGBA8888>>);
static_assert(!CanRound<Haio::Image<Haio::Color::RGB888>>);

auto main() -> int {
    // a crop on three byte pixels, which used to need a round trip through rgba8888
    Haio::Image<Haio::Color::RGB888> wide{4, 2, std::vector<uint8_t>(4 * 2 * 3)};
    for (size_t at = 0; at < wide.data.size(); at++) wide.data[at] = static_cast<uint8_t>(at);

    const auto cropped = Haio::cropImage(wide, Haio::Rect{1, 0, 2, 2});
    check(cropped.width == 2 && cropped.height == 2, "a crop keeps the rectangle it was given");
    check(cropped.data.size() == 2 * 2 * 3, "a crop of rgb888 is three bytes a pixel");
    // row 0 starts at pixel 1, so bytes 3,4,5; row 1 starts at pixel 5, so bytes 15,16,17
    check(cropped.data[0] == 3 && cropped.data[1] == 4 && cropped.data[2] == 5, "the first row lands at the right offset");
    check(cropped.data[6] == 15 && cropped.data[7] == 16 && cropped.data[8] == 17, "the second row lands at the right offset");

    const auto small = Haio::resizeImage(wide, Haio::Size{2, 1});
    check(small.width == 2 && small.height == 1, "a resize honours the size asked for");
    check(small.data.size() == 2 * 1 * 3, "a resize of rgb888 is three bytes a pixel");

    // and the rgba8888 path still does what it always did
    Haio::Image<Haio::Color::RGBA8888> square{8, 8, std::vector<uint8_t>(8 * 8 * 4, 0xFF)};
    const auto rounded = Haio::roundImageCorners(square, 4);
    check(rounded.data[3] == 0, "the top left corner loses its alpha");
    const auto middle = (static_cast<size_t>(4) * 8 + 4) * 4 + 3;
    check(rounded.data[middle] == 0xFF, "the middle keeps its alpha");

    // the two ways of asking a colour how wide it is must not disagree
    for (const auto color : {Haio::Color::RGBA8888, Haio::Color::RGB888, Haio::Color::RGB565, Haio::Color::GRAY8}) {
        const auto measured = Haio::sizeOf(color, Haio::Size{4, 2});
        check(measured && *measured == 8 * Haio::strideOf(color), "sizeOf agrees with strideOf");
    }

    if (failures == 0) std::cout << "transform concepts: ok\n";
    return failures == 0 ? 0 : 1;
}

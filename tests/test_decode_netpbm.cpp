#include <haio.hpp>

#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (ok) return;
    std::cerr << "fail: " << what << '\n';
    failures++;
}

std::vector<uint8_t> bytesOf(std::string_view text) {
    return {text.begin(), text.end()};
}

Haio::Result<Haio::Image<Haio::Color::RGBA8888>> decode(std::vector<uint8_t> data) {
    return Haio::Decode(Haio::Blob{Haio::Format::RAW, Haio::Color::RGBA8888, {}, {}, std::move(data)});
}

/** the same four by two picture, once per member of the family */
void checkAgrees(const std::string& what, std::vector<uint8_t> left, std::vector<uint8_t> right) {
    const auto a = decode(std::move(left));
    const auto b = decode(std::move(right));
    if (!a || !b) {
        check(false, what + " both decode");
        return;
    }
    check(a->width == 4 && a->height == 2, what + " keeps its dimensions");
    check(a->data == b->data, what + " agrees with its ascii or binary twin");
}

}

auto main() -> int {
    // a bitmap calls 1 black, so the first pixel, a 0, comes out white
    const auto p1 = bytesOf("P1\n4 2\n0 1 0 1\n1 0 1 0\n");
    const std::vector<uint8_t> p4 = {'P', '4', '\n', '4', ' ', '2', '\n', 0x50, 0xA0};
    checkAgrees("bitmap", p1, p4);
    {
        const auto decoded = decode(p1);
        check(decoded && decoded->data[0] == 255 && decoded->data[4] == 0,
              "a zero bit is white and a one bit is black");
    }

    const auto p2 = bytesOf("P2\n# a comment sits here\n4 2\n255\n0 85 170 255\n255 170 85 0\n");
    const std::vector<uint8_t> p5 = {'P', '5', '\n', '4', ' ', '2', '\n', '2', '5', '5', '\n',
                                     0, 85, 170, 255, 255, 170, 85, 0};
    checkAgrees("graymap", p2, p5);

    const auto p3 = bytesOf("P3\n4 2\n255\n255 0 0 0 255 0 0 0 255 255 255 255 "
                            "0 0 0 255 255 0 0 255 255 255 0 255\n");
    const std::vector<uint8_t> p6 = {'P', '6', '\n', '4', ' ', '2', '\n', '2', '5', '5', '\n',
                                     255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255,
                                     0, 0, 0, 255, 255, 0, 0, 255, 255, 255, 0, 255};
    checkAgrees("pixmap", p3, p6);

    // a smaller maxval scales up rather than clipping
    checkAgrees("maxval scaling", bytesOf("P2\n4 2\n15\n0 5 10 15\n15 10 5 0\n"), p2);

    // sixteen bit samples are big endian
    std::vector<uint8_t> wide = {'P', '5', '\n', '4', ' ', '2', '\n', '6', '5', '5', '3', '5', '\n'};
    for (const uint16_t sample : {0, 21845, 43690, 65535, 65535, 43690, 21845, 0}) {
        wide.push_back(static_cast<uint8_t>(sample >> 8));
        wide.push_back(static_cast<uint8_t>(sample & 0xFF));
    }
    checkAgrees("sixteen bit graymap", std::move(wide), p2);

    // the colour and grey pipes must not answer for each other
    check(Haio::Detect(p6).color == Haio::Color::RGB888, "a pixmap detects as rgb888");
    check(Haio::Detect(p5).color == Haio::Color::GRAY8, "a graymap detects as gray8");

    // a header promising more than the file holds is refused, not read past
    check(!decode(bytesOf("P2\n4 2\n255\n0 85\n")), "a short ascii body is refused");
    check(!decode(std::vector<uint8_t>{'P', '5', '\n', '4', ' ', '2', '\n', '2', '5', '5', '\n', 0, 1}),
          "a short binary body is refused");
    check(!decode(bytesOf("P5\n99999 99999\n255\n")), "an oversized header is refused");
    check(!decode(bytesOf("P2\n4 2\n0\n")), "a zero maxval is refused");

    if (failures == 0) std::cout << "netpbm: ok\n";
    return failures == 0 ? 0 : 1;
}

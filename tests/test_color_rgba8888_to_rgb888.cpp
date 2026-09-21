#include <haio.hpp>

#include <cstdint>
#include <iostream>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (ok) return;
    std::cerr << "fail: " << what << '\n';
    failures++;
}

/** every fourth byte is alpha and must not survive */
std::vector<uint8_t> makeRgba(size_t pixels) {
    std::vector<uint8_t> out(pixels * 4);
    for (size_t at = 0; at < pixels; at++) {
        out[at * 4 + 0] = static_cast<uint8_t>(at * 7 + 1);
        out[at * 4 + 1] = static_cast<uint8_t>(at * 13 + 2);
        out[at * 4 + 2] = static_cast<uint8_t>(at * 29 + 3);
        out[at * 4 + 3] = static_cast<uint8_t>(at * 31 + 4);
    }
    return out;
}

std::vector<uint8_t> oracle(const std::vector<uint8_t>& src, size_t pixels) {
    std::vector<uint8_t> out(pixels * 3);
    for (size_t at = 0; at < pixels; at++) {
        out[at * 3 + 0] = src[at * 4 + 0];
        out[at * 3 + 1] = src[at * 4 + 1];
        out[at * 3 + 2] = src[at * 4 + 2];
    }
    return out;
}

/**
 * the shuffle constant cannot be checked by running it: this test has to pass on a
 * machine without avx2, where that path never dispatches. so the mask is checked
 * against the documented semantics instead, per 128 bit lane, which is where a wrong
 * constant would actually hide.
 */
void checkAvx2Mask() {
    constexpr signed char mask[32] = {
         0,  1,  2,  4,  5,  6,  8,  9, 10, 12, 13, 14, -1, -1, -1, -1,
         0,  1,  2,  4,  5,  6,  8,  9, 10, 12, 13, 14, -1, -1, -1, -1};

    const auto src = makeRgba(8);
    std::vector<uint8_t> shuffled(32);
    for (int lane = 0; lane < 2; lane++) {
        for (int at = 0; at < 16; at++) {
            const auto selector = mask[lane * 16 + at];
            shuffled[lane * 16 + at] =
                (selector & static_cast<signed char>(0x80)) ? 0 : src[lane * 16 + (selector & 15)];
        }
    }

    // the two twelve byte stores, exactly as Move lays them down
    std::vector<uint8_t> packed(24);
    std::copy_n(shuffled.begin(), 12, packed.begin());
    std::copy_n(shuffled.begin() + 16, 12, packed.begin() + 12);

    check(packed == oracle(src, 8), "avx2 shuffle mask packs eight pixels in order");
}

}

auto main() -> int {
    // one pixel wide so the count drives the split: avx2 blocks, ssse3 blocks, scalar tail
    for (size_t pixels = 0; pixels <= 40; pixels++) {
        const auto src = makeRgba(pixels);
        std::vector<uint8_t> dst(pixels * 3, 0xAA);

        const auto moved = Haio::Codecs::Move<Haio::Color::RGBA8888, Haio::Color::RGB888>(
            src, dst, Haio::Size{1, static_cast<int>(pixels)});

        check(moved.has_value(), "move of " + std::to_string(pixels) + " pixels reports success");
        check(dst == oracle(src, pixels), "move of " + std::to_string(pixels) + " pixels matches scalar");
    }

    // large enough that the wide paths carry the bulk of the work
    {
        const size_t pixels = 1021;  // prime, so every path ends on a ragged tail
        const auto src = makeRgba(pixels);
        std::vector<uint8_t> dst(pixels * 3, 0xAA);
        const auto moved = Haio::Codecs::Move<Haio::Color::RGBA8888, Haio::Color::RGB888>(
            src, dst, Haio::Size{1021, 1});
        check(moved.has_value(), "move of 1021 pixels reports success");
        check(dst == oracle(src, pixels), "move of 1021 pixels matches scalar");
    }

    // a destination one byte short must be refused, not written past
    {
        const auto src = makeRgba(4);
        std::vector<uint8_t> dst(11);
        const auto moved = Haio::Codecs::Move<Haio::Color::RGBA8888, Haio::Color::RGB888>(src, dst, Haio::Size{4, 1});
        check(!moved, "a short destination is refused");
    }

    checkAvx2Mask();

    if (failures == 0) std::cout << "rgba8888 to rgb888: ok\n";
    return failures == 0 ? 0 : 1;
}

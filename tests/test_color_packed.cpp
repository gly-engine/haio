#include <haio.hpp>

#include <iostream>
#include <string>
#include <vector>

using namespace Haio;

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (ok) return;
    std::cerr << "fail: " << what << '\n';
    failures++;
}

Image<Color::RGBA8888> pixels(std::vector<uint8_t> data) {
    return Image<Color::RGBA8888>{static_cast<int>(data.size() / 4), 1, std::move(data)};
}

/**
 * the bytes a colour is actually made of.
 *
 * this is the half a roundtrip cannot check: a pair of conversions that disagree with
 * the world in the same way agrees with itself perfectly, and these layouts exist
 * precisely so that a tga decoder can copy rather than convert. so the layout is
 * written down here as bytes, in the order the file has them.
 */
void checkPacks(const std::string& what, Color to, Image<Color::RGBA8888> from, std::vector<uint8_t> want) {
    const auto packed = Convert(from, to);
    if (!packed) {
        check(false, what + " converts");
        return;
    }
    check(*packed == want, what);
}

/** and the way back, for the values the narrower colour can hold exactly */
void checkRoundTrip(const std::string& what, Color to, std::vector<uint8_t> data) {
    const auto packed = Convert(pixels(data), to);
    if (!packed) {
        check(false, what + " converts");
        return;
    }

    const Size size{static_cast<int>(data.size() / 4), 1};
    const auto want = sizeOf(to, size);
    check(want && *want == packed->size(), what + " is as long as its colour says");

    std::vector<uint8_t> back(data.size());
    bool moved = false;
    HAIO_FOR_EACH_COLOR(c) {
        constexpr Color color = std::meta::extract<Color>(c);
        if constexpr (Codecs::Movable<color, Color::RGBA8888>) {
            if (color != to) continue;
            moved = Codecs::Move<color, Color::RGBA8888>(*packed, back, size).has_value();
        }
    }
    check(moved && back == data, what + " comes back as it went");
}

}

auto main() -> int {
    // one pixel of each, in bytes, with the file's order on the right
    checkPacks("bgr888 puts blue first", Color::BGR888, pixels({1, 2, 3, 255}), {3, 2, 1});
    checkPacks("bgra8888 keeps the alpha where it was", Color::BGRA8888, pixels({1, 2, 3, 4}), {3, 2, 1, 4});

    // 0 11111 00000 00000, little endian, which is what a sixteen bit tga stores
    checkPacks("rgb555 packs red into the top five bits", Color::RGB555, pixels({255, 0, 0, 255}), {0x00, 0x7c});
    // 11111 00000 00000 1: the same five bits shifted up to make room for the alpha
    checkPacks("rgba5551 keeps its alpha at the bottom", Color::RGBA5551, pixels({255, 0, 0, 255}), {0x01, 0xf8});
    checkPacks("and a clear pixel clears it", Color::RGBA5551, pixels({255, 0, 0, 0}), {0x00, 0xf8});

    // an alpha of one bit rounds at half, so this is the whole of what it can say
    checkPacks("an alpha over half is there", Color::RGBA5551, pixels({0, 0, 0, 128}), {0x01, 0x00});
    checkPacks("and under half it is not", Color::RGBA5551, pixels({0, 0, 0, 127}), {0x00, 0x00});

    checkRoundTrip("bgr888", Color::BGR888, {0, 1, 2, 255, 253, 254, 255, 255});
    checkRoundTrip("bgra8888", Color::BGRA8888, {0, 1, 2, 3, 252, 253, 254, 255});

    /**
     * five bits reach 255 by repeating themselves, so the values that survive are the
     * ones that pattern produces: 0, 8, 16 ... 255. anything else is rounded to the
     * nearest of those, which is the point of the colour and not a defect of it.
     */
    checkRoundTrip("rgb555", Color::RGB555, {0, 8, 16, 255, 231, 247, 255, 255});
    checkRoundTrip("rgba5551", Color::RGBA5551, {0, 8, 16, 255, 231, 247, 255, 0});

    // the narrow colours are two bytes a pixel and the wide ones three or four, which
    // is what lets a crop or a resize index them
    check(strideOf(Color::RGB555) == 2 && strideOf(Color::RGBA5551) == 2, "the fives are two bytes a pixel");
    check(strideOf(Color::BGR888) == 3 && strideOf(Color::BGRA8888) == 4, "and the eights are three and four");
    check(alphaOffsetOf(Color::BGRA8888) == 3, "bgra keeps its alpha in the last byte");
    check(alphaOffsetOf(Color::BGR888) < 0, "and the one without an alpha says so");

    // the names ffmpeg uses reach the same colours, since -pix_fmt borrowed the option
    check(colorNamed("bgr24") == Color::BGR888 && colorNamed("bgra") == Color::BGRA8888,
          "ffmpeg's names for the two windows layouts");
    check(colorNamed("yuv420p") == Color::YUV420 && colorNamed("pal8") == Color::PALETTE,
          "and for the planar and the indexed ones");
    check(colorNamed("rgb555le") == Color::RGB555 && colorNamed("rgb555") == Color::RGB555,
          "with or without the endianness it never had a choice about");
    check(!colorNamed("yuv444p"), "a colour haio does not have is not quietly something else");

    if (failures == 0) std::cout << "packed colours: ok\n";
    return failures == 0 ? 0 : 1;
}

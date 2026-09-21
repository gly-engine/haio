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

void append(std::vector<uint8_t>& out, std::initializer_list<int> bytes) {
    for (const auto byte : bytes) out.push_back(static_cast<uint8_t>(byte));
}

/**
 * a tga header written out by hand, so a test says what it is testing rather than
 * calling the encoder and comparing it with itself.
 */
std::vector<uint8_t> header(int type, int width, int height, int depth, int descriptor,
                            int mapCount = 0, int mapBits = 0) {
    std::vector<uint8_t> out;
    append(out, {0, mapCount == 0 ? 0 : 1, type});
    append(out, {0, 0, mapCount & 0xff, (mapCount >> 8) & 0xff, mapBits});
    append(out, {0, 0, 0, 0});
    append(out, {width & 0xff, (width >> 8) & 0xff, height & 0xff, (height >> 8) & 0xff});
    append(out, {depth, descriptor});
    return out;
}

Blob blobOf(std::vector<uint8_t> data) {
    return Blob{Format::RAW, Color::RGBA8888, {}, {}, std::move(data)};
}

/** what the runtime path gives back: full colour, whatever the file held */
Result<Image<Color::RGBA8888>> decode(std::vector<uint8_t> data) {
    return Decode(blobOf(std::move(data)));
}

bool pixelIs(const Image<Color::RGBA8888>& image, int x, int y, int r, int g, int b, int a = 255) {
    const auto at = (static_cast<size_t>(y) * image.width + x) * 4;
    return image.data[at + 0] == r && image.data[at + 1] == g && image.data[at + 2] == b
        && image.data[at + 3] == a;
}

}

auto main() -> int {
    // a file this small is not a tga, whatever its first bytes happen to be
    check(!Detect(std::vector<uint8_t>{0, 0, 2, 0}), "eighteen bytes are the least a tga can be");
    {
        auto broken = header(2, 2, 2, 24, 0x20);
        broken[1] = 7;   // a colour map type nobody defined
        broken.resize(broken.size() + 12);
        check(!Detect(broken), "a header that disagrees with itself is not recognised");
    }

    /**
     * a truecolor tga with no origin bit, which is the common case: the file starts at
     * the bottom left and the picture haio hands back starts at the top left.
     */
    {
        auto file = header(2, 2, 2, 24, 0);
        append(file, {0, 0, 255, 0, 255, 0});         // bottom row: red, green
        append(file, {255, 0, 0, 0, 0, 255});         // the row above it: blue, red
        const auto found = Detect(file);
        check(found.format == Format::TGA && found.color == Color::BGR888, "24 bits a pixel is bgr888");

        const auto image = decode(file);
        check(image && image->width == 2 && image->height == 2, "it keeps its dimensions");
        check(image && pixelIs(*image, 0, 0, 0, 0, 255), "the last stored row is the first one shown");
        check(image && pixelIs(*image, 0, 1, 255, 0, 0), "and the first stored row is the last");
    }

    /** the same picture stored top left first, which is the descriptor's bit five */
    {
        auto file = header(2, 2, 1, 24, 0x20);
        append(file, {0, 0, 255, 0, 255, 0});
        const auto image = decode(file);
        check(image && pixelIs(*image, 0, 0, 255, 0, 0), "an origin at the top is left alone");
    }

    /** right to left, which is bit four and far rarer than the spec suggests */
    {
        auto file = header(2, 2, 1, 24, 0x30);
        append(file, {0, 0, 255, 0, 255, 0});
        const auto image = decode(file);
        check(image && pixelIs(*image, 0, 0, 0, 255, 0), "a row stored backwards comes back forwards");
    }

    /** thirty two bits, where the fourth byte is an alpha because the header says so */
    {
        auto file = header(2, 1, 1, 32, 0x28);
        append(file, {10, 20, 30, 40});
        const auto found = Detect(file);
        check(found.format == Format::TGA && found.color == Color::BGRA8888, "32 bits a pixel is bgra8888");

        const auto image = decode(file);
        check(image && pixelIs(*image, 0, 0, 30, 20, 10, 40), "blue comes first and the alpha survives");
    }

    /**
     * sixteen bits, which is the same two bytes read two ways: the attribute bits are
     * the whole difference between a colour with an alpha and one without.
     */
    {
        // 0 11111 00000 00000 in the format's own A1R5G5B5: red, with the top bit clear
        auto opaque = header(2, 1, 1, 16, 0x21);
        append(opaque, {0x00, 0x7c});
        check(Detect(opaque).color == Color::RGBA5551, "an attribute bit makes it rgba5551");
        const auto lit = decode(opaque);
        check(lit && pixelIs(*lit, 0, 0, 255, 0, 0, 0), "and the top bit, which is clear, is the alpha");

        auto plain = header(2, 1, 1, 16, 0x20);
        append(plain, {0x00, 0x7c});
        check(Detect(plain).color == Color::RGB555, "without one it is rgb555");
        const auto flat = decode(plain);
        check(flat && pixelIs(*flat, 0, 0, 255, 0, 0, 255), "and every pixel is opaque");
    }

    /** runs, which is what most tga files in the world actually are */
    {
        auto file = header(10, 4, 1, 32, 0x28);
        append(file, {0x82, 1, 2, 3, 255});           // three of the same
        append(file, {0x00, 9, 8, 7, 255});           // then one of its own
        const auto image = decode(file);
        check(image && image->width == 4, "a run length picture is as wide as its header says");
        check(image && pixelIs(*image, 0, 0, 3, 2, 1) && pixelIs(*image, 2, 0, 3, 2, 1),
              "a run repeats one pixel");
        check(image && pixelIs(*image, 3, 0, 7, 8, 9), "and a raw packet copies them one by one");

        // the same run against a picture too small to hold it, which is a broken file
        // rather than something to trim
        auto overrun = header(10, 2, 1, 32, 0x28);
        append(overrun, {0x82, 1, 2, 3, 255});
        check(!decode(overrun), "a run that carries past the end is refused");
    }

    /** a colour map, which is the format's own palette */
    {
        auto file = header(1, 2, 1, 8, 0x20, 3, 24);
        append(file, {255, 0, 0});                    // colour 0, stored blue first
        append(file, {0, 255, 0});
        append(file, {0, 0, 255});
        append(file, {2, 0});
        const auto found = Detect(file);
        check(found.format == Format::TGA && found.color == Color::PALETTE, "a mapped tga is a palette");

        const auto indexed = Codecs::Decode<Format::TGA, Color::PALETTE>(blobOf(file));
        check(indexed && indexed->entries.size() == 3, "it carries the colours it indexes");
        check(indexed && indexed->data[0] == 2 && indexed->data[1] == 0, "and one index per pixel");

        const auto image = decode(file);
        check(image && pixelIs(*image, 0, 0, 255, 0, 0), "the runtime path resolves them");
        check(image && pixelIs(*image, 1, 0, 0, 0, 255), "in the order the map stored them");
    }

    /** one byte a pixel and no colour at all */
    {
        auto file = header(3, 2, 1, 8, 0x20);
        append(file, {0, 128});
        check(Detect(file).color == Color::GRAY8, "a grey tga is gray8");
        const auto image = decode(file);
        check(image && pixelIs(*image, 1, 0, 128, 128, 128), "and it comes back as a level of grey");
    }

    /**
     * out and back again. the picture has an alpha and a colour that survives five
     * bits, so the same comparison works for every colour the container writes.
     */
    {
        const Image<Color::RGBA8888> picture{2, 2, {255, 0, 0, 255,   0, 255, 0, 255,
                                                    0, 0, 255, 0,     255, 255, 255, 255}};
        for (const auto colour : {Color::BGRA8888, Color::BGR888, Color::RGB555, Color::RGBA5551}) {
            const auto name = std::string(colorName(colour));
            const auto written = Encode(picture, Format::TGA, colour);
            check(written.has_value(), "a tga holding " + name + " is written");
            if (!written) continue;

            const auto found = Detect(written->data);
            check(found.format == Format::TGA && found.color == colour,
                  "a tga holding " + name + " says so to the next reader");

            const auto back = decode(written->data);
            check(back && back->width == 2 && back->height == 2, name + " keeps its dimensions");
            check(back && pixelIs(*back, 0, 0, 255, 0, 0), name + " keeps a red pixel red");
            check(back && pixelIs(*back, 1, 1, 255, 255, 255), name + " keeps the corner opposite");
        }

        // nobody named a colour, so the container's own answer is the one that keeps
        // everything the picture arrived with
        const auto plain = Encode(picture, Format::TGA);
        check(plain && Detect(plain->data).color == Color::BGRA8888, "a bare tga is written bgra8888");

        const auto alpha = decode(plain->data);
        check(alpha && pixelIs(*alpha, 0, 1, 0, 0, 255, 0), "which is the only one that keeps an alpha");

        // a container that holds one colour says no rather than quietly writing another
        check(!Encode(picture, Format::PNG, Color::RGB565), "a png does not hold rgb565");
    }

    /** indices out and back, which is the only colour that carries something extra */
    {
        Image<Color::PALETTE> indexed{2, 1, {1, 0}};
        indexed.entries = {0x00ff0000, 0x000000ff};

        const auto written = Codecs::Encode<Format::TGA, Color::PALETTE>(indexed);
        check(written.has_value(), "a palette picture is written as one");
        if (written) {
            const auto back = Codecs::Decode<Format::TGA, Color::PALETTE>(blobOf(written->data));
            check(back && back->data == indexed.data, "the indices come back as they went");
            check(back && back->entries == std::vector<uint32_t>{0xffff0000, 0xff0000ff},
                  "and so do the colours, opaque, in the order they were in");
        }

        // an index the palette does not reach is a broken picture, not a black pixel
        Image<Color::PALETTE> broken{1, 1, {7}};
        broken.entries = {0x00ff0000};
        check(!Codecs::Encode<Format::TGA, Color::PALETTE>(broken), "an index past the palette is refused");
    }

    if (failures == 0) std::cout << "codec tga: ok\n";
    return failures == 0 ? 0 : 1;
}

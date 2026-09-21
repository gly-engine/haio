#include <haio.hpp>
#include <haio_palette.hpp>

#include <iostream>
#include <string>

using namespace Haio;

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (ok) return;
    std::cerr << "fail: " << what << '\n';
    failures++;
}

Image<Color::RGBA8888> solid(int width, int height, uint32_t colour) {
    std::vector<uint8_t> data(static_cast<size_t>(width) * height * 4);
    for (size_t at = 0; at < data.size(); at += 4) {
        data[at + 0] = static_cast<uint8_t>((colour >> 16) & 0xFF);
        data[at + 1] = static_cast<uint8_t>((colour >> 8) & 0xFF);
        data[at + 2] = static_cast<uint8_t>(colour & 0xFF);
        data[at + 3] = 0xFF;
    }
    return Image<Color::RGBA8888>{width, height, std::move(data)};
}

}

auto main() -> int {
    // a bank is as wide as the picture needs, which is the whole reason the count is
    // passed in: the same spelling means different colours for different pictures
    {
        const auto four = paletteNamed("tic80:1", 4);
        const auto next = paletteNamed("tic80:2", 4);
        check(four && four->size() == 4, "a bank of four gives four colours");
        check(next && next->size() == 4, "so does the next one");
        check(four && next && (*four)[0] != (*next)[0], "and they are different colours");

        const auto three = paletteNamed("tic80:2", 3);
        const auto spelled = paletteNamed("tic80:4,5,6", 0);
        check(three && spelled && *three == *spelled,
              "bank 2 of a three colour picture is colours 4,5,6, counted from one");
    }

    // the whole palette when nothing is sliced
    {
        const auto all = paletteNamed("cga", 0);
        check(all && all->size() == 16, "cga has sixteen colours");
        const auto nes = paletteNamed("nes", 0);
        check(nes && nes->size() == 64, "the nes master has sixty four");
        const auto koko = paletteNamed("kokobatoru", 0);
        check(koko && koko->size() == 4, "a chrfonts palette has the four a tile can hold");
    }

    // what cannot work says so rather than guessing
    check(!paletteNamed("nosuch", 4), "an unknown palette is refused");
    check(!paletteNamed("cga:0", 4), "bank zero is refused, since banks count from one");
    check(!paletteNamed("cga:9", 4), "a bank past the end is refused");
    check(!paletteNamed("cga:", 4), "a colon with nothing after it is refused");

    // a span, which is what makes asking for the first three bearable
    {
        const auto span = paletteNamed("tic80:1..3", 0);
        const auto spelled = paletteNamed("tic80:1,2,3", 0);
        check(span && span->size() == 3, "a span gives every colour in it");
        check(span && spelled && *span == *spelled, "and the same ones as spelling them out");

        const auto mixed = paletteNamed("tic80:1..3,7", 0);
        check(mixed && mixed->size() == 4, "a span and a list together give both");

        check(!paletteNamed("tic80:3..1", 0), "a span that runs backwards is refused");
        check(!paletteNamed("tic80:1..99", 0), "a span past the end is refused");
    }
    check(!paletteNamed("cga:99", 0), "a colour past the end is refused");

    // the filter is not optional, and each one answers a different question
    check(ditherNamed("nearest").has_value(), "nearest is a filter");
    check(ditherNamed("bayer").has_value(), "bayer is a filter");
    check(ditherNamed("floyd").has_value(), "floyd is a filter");
    check(ditherNamed("error").has_value(), "error is a filter");
    check(!ditherNamed("point").has_value(), "and something else is not");

    const std::vector<uint32_t> blackWhite = {0x000000, 0xFFFFFF};

    /**
     * a colour already in the palette comes through untouched under the filters that
     * decide a pixel on its own, and not under the ones that dither.
     *
     * that is not a shortcoming: an ordered or diffused dither perturbs every pixel
     * by design, which is the whole mechanism. it is also why the filter is not
     * optional, and why pixel art wants nearest or error rather than either dither.
     */
    {
        for (const auto how : {Dither::Nearest, Dither::Strict}) {
            const auto done = toPalette(solid(8, 8, 0xFFFFFF), blackWhite, how);
            check(done.has_value(), "a palette colour converts");
            if (done) {
                const bool allWhite = std::ranges::all_of(done->data, [](uint8_t i) { return i == 1; });
                check(allWhite, "and stays the colour it was");
            }
        }
        for (const auto how : {Dither::Bayer, Dither::Floyd}) {
            const auto done = toPalette(solid(8, 8, 0xFFFFFF), blackWhite, how);
            check(done.has_value(), "a dither converts it too");
        }
    }

    // strict is the one that refuses rather than approximates
    {
        const auto grey = solid(4, 4, 0x808080);
        check(!toPalette(grey, blackWhite, Dither::Strict), "strict refuses a colour that is not there");

        const auto near = toPalette(grey, blackWhite, Dither::Nearest);
        check(near.has_value(), "nearest takes the closest one instead");
        if (near) {
            const bool flat = std::ranges::all_of(near->data, [&](uint8_t i) { return i == near->data[0]; });
            check(flat, "and gives every pixel the same answer, since the input was flat");
        }
    }

    // a dither trades flatness for a pattern: the same grey becomes both colours
    {
        const auto dithered = toPalette(solid(8, 8, 0x808080), blackWhite, Dither::Bayer);
        check(dithered.has_value(), "bayer converts");
        if (dithered) {
            const bool mixed = std::ranges::any_of(dithered->data, [](uint8_t i) { return i == 0; })
                            && std::ranges::any_of(dithered->data, [](uint8_t i) { return i == 1; });
            check(mixed, "and a flat grey comes out as a mix of both colours");
        }
    }

    // the palette travels with the picture, which is the point of the specialisation
    {
        const auto made = toPalette(solid(8, 8, 0x000000), blackWhite, Dither::Nearest);
        check(made && made->entries.size() == 2, "the picture carries its palette");

        const auto cropped = cropImage(*made, Rect{0, 0, 4, 4});
        check(cropped.entries == made->entries, "and keeps it through a crop");
        check(cropped.width == 4, "which did crop it");

        const auto back = Codecs::Convert<Color::PALETTE, Color::RGBA8888>(*made);
        check(back.has_value(), "indices become colours again");
        if (back) check(back->data[0] == 0 && back->data[3] == 0xFF, "and the colour is the one indexed");
    }

    // indices with no palette are not a picture yet
    {
        Image<Color::PALETTE> orphan{2, 2, {0, 0, 0, 0}, {}};
        check(!Codecs::Convert<Color::PALETTE, Color::RGBA8888>(orphan),
              "indices without a palette are refused, not drawn black");
    }

    if (failures == 0) std::cout << "palette: ok\n";
    return failures == 0 ? 0 : 1;
}

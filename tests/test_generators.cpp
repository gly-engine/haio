#include <haio.hpp>

#include <iostream>
#include <string>

namespace {

using namespace Haio;

int failures = 0;

void check(bool ok, const std::string& what) {
    if (ok) return;
    std::cerr << "fail: " << what << '\n';
    failures++;
}

/** a picture painted by a brush, as the command line would ask for it */
Image<Color::RGBA8888> draw(Brush brush, std::string_view words, Settings settings = {}) {
    auto native = GenerateNative(brush, words, settings);
    check(native.has_value(), std::string(brushName(brush)) + ":" + std::string(words) + " paints");
    if (!native) return {};
    auto rgba = Convert(*std::move(native), Color::RGBA8888);
    return rgba ? Image<Color::RGBA8888>{rgba->width, rgba->height, std::move(rgba->data)} : Image<Color::RGBA8888>{};
}

uint32_t at(const Image<Color::RGBA8888>& image, int x, int y) {
    const auto* p = image.data.data() + (static_cast<size_t>(y) * static_cast<size_t>(image.width) + static_cast<size_t>(x)) * 4;
    return (uint32_t{p[3]} << 24) | (uint32_t{p[0]} << 16) | (uint32_t{p[1]} << 8) | p[2];
}

Settings sized(std::string size) { return {Setting{"size", std::move(size)}}; }

}

/**
 * every value below is what imagemagick 6.9.12 draws for the same line, read off
 * `convert ... -depth 8 txt:-`. the generators are meant to be the same picture pixel
 * for pixel, so these are exact rather than close.
 */
auto main() -> int {
    // top to bottom, rounded the way its sixteen bit quantum rounds
    const auto linear = draw(Brush::Gradient, "red-blue", sized("1x5"));
    check(at(linear, 0, 0) == 0xFFFF0000 && at(linear, 0, 1) == 0xFFBF003F && at(linear, 0, 2) == 0xFF7F007F
              && at(linear, 0, 4) == 0xFF0000FF, "gradient:red-blue runs top to bottom");

    const auto odd = draw(Brush::Gradient, "#123456-#fedcba", sized("1x6"));
    check(at(odd, 0, 1) == 0xFF41556A, "a blend lands where imagemagick rounds it");

    // one colour fades to white when it is dark and to black when it is bright
    check(at(draw(Brush::Gradient, "blue", sized("1x3")), 0, 2) == 0xFFFFFFFF, "gradient:blue fades to white");
    check(at(draw(Brush::Gradient, "#ff8800", sized("1x3")), 0, 2) == 0xFF000000, "gradient:#ff8800 fades to black");

    // a single row runs across rather than being one colour
    check(at(draw(Brush::Gradient, "", sized("5x1")), 1, 0) == 0xFFBFBFBF, "a 5x1 gradient runs across");

    // transparency blends apart from the colours, which are weighted by it
    const auto clear = draw(Brush::Gradient, "#ff000080-#0000ff", sized("1x5"));
    check(at(clear, 0, 1) == 0xA0990065, "a half transparent end is blended as imagemagick blends it");

    const auto northEast = draw(Brush::Gradient, "black-white", {Setting{"size", "8x5"}, Setting{"gradient:direction", "NorthEast"}});
    check(at(northEast, 0, 0) == 0xFF3E3E3E && at(northEast, 7, 0) == 0xFFFFFFFF && at(northEast, 0, 4) == 0xFF000000,
          "northeast runs corner to corner");

    const auto radial = draw(Brush::RadialGradient, "", sized("7x3"));
    check(at(radial, 3, 1) == 0xFFFFFFFF && at(radial, 2, 1) == 0xFFAAAAAA && at(radial, 1, 0) == 0xFF404040
              && at(radial, 0, 1) == 0xFF000000, "radial-gradient reaches the larger half");

    const auto diagonal = draw(Brush::RadialGradient, "", {Setting{"size", "7x3"}, Setting{"gradient:extent", "Diagonal"}});
    check(at(diagonal, 0, 1) == 0xFF0D0D0D, "a diagonal extent reaches the corners");

    const auto hald = draw(Brush::Hald, "2");
    check(hald.width == 8 && hald.height == 8 && at(hald, 1, 0) == 0xFF550000 && at(hald, 4, 0) == 0xFF005500,
          "hald:2 is 8x8, red fastest");

    const auto nothing = draw(Brush::Null, "", sized("3x2"));
    check(nothing.width == 3 && at(nothing, 2, 1) == 0x00000000, "null: is transparent at any size");

    // the noise is haio's own, so all that can be held to is that a seed repeats itself
    const Settings seeded{Setting{"size", "33x17"}, Setting{"seed", "7"}};
    check(draw(Brush::Plasma, "", seeded).data == draw(Brush::Plasma, "", seeded).data, "a seed draws the same plasma twice");
    check(draw(Brush::Plasma, "fractal", seeded).data != draw(Brush::Plasma, "", seeded).data,
          "plasma:fractal starts from other corners");

    // a code is its modules and a one module margin: "tiny" is a version 1 qr code,
    // 21 modules a side, and without -size a module is a pixel
    const auto qr = draw(Brush::Qr, "tiny");
    check(qr.width == 23 && qr.height == 23, "qr:tiny is its 21 modules and a margin of one");
    check(at(qr, 0, 0) == 0xFFFFFFFF && at(qr, 1, 1) == 0xFF000000 && at(qr, 8, 8) == 0xFFFFFFFF,
          "a finder pattern starts where the margin ends");

    const auto bare = draw(Brush::Qr, "tiny", {Setting{"margin", "0"}});
    check(bare.width == 21 && at(bare, 0, 0) == 0xFF000000, "-margin 0 is the code alone");
    const auto wide = draw(Brush::Qr, "tiny", {Setting{"margin", "4"}});
    check(wide.width == 29 && at(wide, 3, 3) == 0xFFFFFFFF && at(wide, 4, 4) == 0xFF000000, "-margin 4 is the standard's");

    // with -size, the largest whole module that fits with its margin, centred: 23 in 64 is two
    const auto scaled = draw(Brush::Qr, "tiny", sized("64x64"));
    check(scaled.width == 64 && at(scaled, 10, 10) == 0xFFFFFFFF && at(scaled, 11, 11) == 0xFF000000,
          "a scaled code keeps its modules whole, its margin with them");

    // a code is grey, black on opaque white; colour comes from tinting it
    check(GenerateNative(Brush::Qr, "tiny", {})->color == Color::GRAYALPHA88, "qr: paints in grayalpha88");
    check(GenerateNative(Brush::Code, "haio", {})->color == Color::GRAYALPHA88, "and so does code:");

    // -hole leaves the middle in the background, grown to whole modules
    const auto holed = draw(Brush::Qr, "http://pudim.com.br", {Setting{"size", "300x300"}, Setting{"hole", "64x64"}});
    bool empty = true;
    for (int y = 150 - 32; y < 150 + 32; y++) {
        for (int x = 150 - 32; x < 150 + 32; x++) empty = empty && at(holed, x, y) == 0xFFFFFFFF;
    }
    check(empty, "-hole leaves a hole in the middle");
    check(!GenerateNative(Brush::Qr, "http://pudim.com.br", {Setting{"size", "300x300"}, Setting{"hole", "150x150"}}),
          "and refuses one that would leave the code unreadable");
    check(!GenerateNative(Brush::Qr, "http://pudim.com.br", {Setting{"size", "155x155"}, Setting{"hole", "64x64"}}),
          "a third of the side each way is the most a hole may be");

    // without -size the hole decides the scale: the fewest pixels a module that make
    // room for it, 8 here, so 29 modules and the margin are 248 pixels
    const auto grown = draw(Brush::Qr, "http://pudim.com.br", {Setting{"hole", "64x64"}});
    bool room = true;
    for (int y = 124 - 32; y < 124 + 32; y++) {
        for (int x = 124 - 32; x < 124 + 32; x++) room = room && at(grown, x, y) == 0xFFFFFFFF;
    }
    check(grown.width == 248 && grown.height == 248 && room, "-hole alone sizes the picture to fit it");

    // a linear code fills the height it is given, one row of bars
    const auto bars = draw(Brush::Code, "haio", {Setting{"size", "300x40"}, Setting{"format", "Code 39"}});
    check(bars.width == 300 && bars.height == 40, "code: is drawn at the size asked for");
    bool same = true;
    for (int x = 0; x < bars.width; x++) same = same && at(bars, x, 0) == at(bars, x, 39);
    check(same, "and every row of it is the same row");

    check(!GenerateNative(Brush::Code, "123", {Setting{"format", "ean8"}}), "an ean-8 is 7 digits or 8");
    check(!GenerateNative(Brush::Qr, "x", {Setting{"size", "10x10"}}), "and a code that does not fit is refused");

    // text is black on white, as big as its lines without -size and centred with one
    const auto words = draw(Brush::Text, "ola mundo", {Setting{"font-size", "32"}});
    bool inked = false;
    for (int y = 0; y < words.height; y++) {
        for (int x = 0; x < words.width; x++) inked = inked || at(words, x, y) == 0xFF000000;
    }
    check(words.height >= 32 && at(words, 0, 0) == 0xFFFFFFFF && inked, "text: is black on white");
    check(draw(Brush::Text, "oi", sized("200x100")).width == 200, "text: is drawn at the size asked for");

    // a style is its words in any order and any case, and one the font lacks is made up
    const auto bold = draw(Brush::Text, "oi", {Setting{"font-size", "32"}, Setting{"font-style", "ItalicBold"}});
    check(bold.data != draw(Brush::Text, "oi", {Setting{"font-size", "32"}}).data, "-font-style ItalicBold is not regular");
    check(bold.data == draw(Brush::Text, "oi", {Setting{"font-size", "32"}, Setting{"font-style", "bold italic"}}).data,
          "and is the same as bold italic");
    check(!GenerateNative(Brush::Text, "oi", {Setting{"font-style", "heavy"}}), "a style it does not know is refused");
    check(!GenerateNative(Brush::Text, "oi", {Setting{"font-name", "No Such Family"}}), "and so is a font it cannot find");

    // the names come off the enumerators, a dash between words, and the aliases off the declarations
    check(brushName(Brush::RadialGradient) == "radial-gradient" && brushNamed("radial-gradient") == Brush::RadialGradient,
          "RadialGradient is spelled radial-gradient");
    check(brushNamed("null") == Brush::Null && brushNamed("Hald") == Brush::Hald, "a brush is named in any case");
    check(brushNamed("canvas") == Brush::Xc && brushNamed("fractal") == Brush::Plasma, "canvas and fractal are aliases");
    check(!brushNamed("radialgradient") && !brushNamed("png"), "and nothing else is a brush");
    static_assert(Codecs::Generatable<Brush::Hald, Color::RGBA8888>, "a brush is declared for the colour it paints in");
    static_assert(!Codecs::Generatable<Brush::Hald, Color::RGB565>, "and only for that one");
    static_assert(Codecs::Generatable<Brush::Qr, Color::GRAYALPHA88> && !Codecs::Generatable<Brush::Qr, Color::RGBA8888>,
                  "a code is painted in grey alone");

    if (failures == 0) std::cout << "generators: ok\n";
    return failures == 0 ? 0 : 1;
}

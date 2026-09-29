#include <haio_util.hpp>

#include <iostream>
#include <string>

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (ok) return;
    std::cerr << "fail: " << what << '\n';
    failures++;
}

void is(std::string_view name, uint32_t argb) {
    const auto found = Haio::Util::GetColorFromName(name);
    check(found && *found == argb, std::string(name) + " is the colour imagemagick gives it");
}

}

auto main() -> int {
    // the ends of the table and the middle, which is where a binary search goes wrong
    is("aliceblue", 0xFFF0F8FF);
    is("yellowgreen", 0xFF9ACD32);
    is("tomato", 0xFFFF6347);

    // the ones people get wrong: the svg green is not the x11 one, and lime is
    is("green", 0xFF008000);
    is("lime", 0xFF00FF00);
    // and imagemagick's own, where it parts from css: gray and grey are not the same
    is("gray", 0xFF7E7E7E);
    is("grey", 0xFFBEBEBE);
    check(!Haio::Util::GetColorFromName("rebeccapurple"), "rebeccapurple is css's, and imagemagick 6 has none");

    is("transparent", 0x00000000);
    is("none", 0x00000000);

    // any case, the way both of them read it
    is("AliceBlue", 0xFFF0F8FF);
    is("TOMATO", 0xFFFF6347);

    // a share of white, with x11's rounding, which gives back every imagemagick value
    is("gray0", 0xFF000000);
    is("gray50", 0xFF7F7F7F);
    is("grey1", 0xFF030303);
    is("gray100", 0xFFFFFFFF);

    is("#f80", 0xFFFF8800);
    is("#ff880080", 0x80FF8800);
    is("#FF8800", 0xFFFF8800);
    is("#ff88", 0x88FFFF88);   // four digits are #rgba

    for (const auto* nothing : {"", "nosuch", "gray101", "gray-1", "grayx", "#ff888", "#gg0000", "fractal"}) {
        check(!Haio::Util::GetColorFromName(nothing), std::string("\"") + nothing + "\" names no colour");
    }

    if (failures == 0) std::cout << "colors: ok\n";
    return failures == 0 ? 0 : 1;
}

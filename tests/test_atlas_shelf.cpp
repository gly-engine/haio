#include <haio_atlas.hpp>

#include <algorithm>
#include <iostream>
#include <string>

using Haio::Rect;
using Haio::Size;
using Haio::Atlas::shelf;

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (ok) return;
    std::cerr << "fail: " << what << '\n';
    failures++;
}

bool same(const Size& a, const Size& b) {
    return a.width == b.width && a.height == b.height;
}

bool overlaps(const Rect& a, const Rect& b) {
    return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height;
}

/** what every layout owes, whatever the sizes: nothing on top of anything, nothing outside */
void checkSound(const std::vector<Size>& sizes, const std::string& what) {
    const auto laid = shelf(sizes);
    check(laid.has_value(), what + " is laid out");
    if (!laid) return;

    check(laid->rects.size() == sizes.size(), what + ": one rectangle per picture");
    for (size_t i = 0; i < laid->rects.size(); i++) {
        const auto& r = laid->rects[i];
        check(r.width == sizes[i].width && r.height == sizes[i].height, what + ": a rectangle keeps its own size");
        check(r.x >= 0 && r.y >= 0 && r.x + r.width <= laid->canvas.width && r.y + r.height <= laid->canvas.height,
              what + ": inside the canvas");
        for (size_t j = i + 1; j < laid->rects.size(); j++) {
            check(!overlaps(r, laid->rects[j]), what + ": nothing lies on top of anything");
        }
    }
}

void testPlacement() {
    const auto four = shelf(std::vector<Size>{{64, 64}, {64, 64}, {64, 64}, {64, 64}});
    check(four.has_value(), "four equal pictures");
    if (four) {
        check(four->canvas.width * four->canvas.height == 4 * 64 * 64, "four 64x64 waste nothing");
        std::vector<Rect> got = four->rects;
        std::ranges::sort(got, [](const Rect& a, const Rect& b) { return a.y != b.y ? a.y < b.y : a.x < b.x; });
        check(got[0].x == 0 && got[0].y == 0 && got[1].x == 64 && got[1].y == 0
              && got[2].x == 0 && got[2].y == 64 && got[3].x == 64 && got[3].y == 64,
              "and they land on the corners of a two by two grid");
    }

    // the point of the cell coming from the smallest: a big one spans several of them
    const auto mixed = shelf(std::vector<Size>{{16, 16}, {48, 48}});
    check(mixed.has_value(), "a small one and a big one");
    if (mixed) {
        check(mixed->rects[0].width == 16 && mixed->rects[1].width == 48, "both keep their size");
        for (const auto& r : mixed->rects) {
            check(r.x % Haio::Atlas::alignment == 0 && r.y % Haio::Atlas::alignment == 0,
                  "and both start on the alignment");
        }
    }
}

void testSound() {
    checkSound({{64, 64}}, "one picture");
    checkSound({{64, 64}, {64, 64}, {64, 64}}, "three equal");
    checkSound({{16, 16}, {48, 48}, {16, 32}, {80, 16}}, "four mixed");
    checkSound({{8, 8}, {8, 8}, {8, 8}, {8, 8}, {8, 8}, {8, 8}, {8, 8}}, "seven tiny");
    checkSound({{100, 7}, {7, 100}, {33, 33}}, "sizes that round badly");
}

void testRefused() {
    check(!shelf(std::vector<Size>{}), "nothing to pack is refused");
    check(!shelf(std::vector<Size>{{0, 10}}), "and so is a picture with no pixels");
}

}

auto main() -> int {
    testPlacement();
    testSound();
    testRefused();

    if (failures == 0) std::cout << "atlas shelf: ok\n";
    return failures == 0 ? 0 : 1;
}

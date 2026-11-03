#include <haio_codec.hpp>
#include <haio/generated/codec.hpp>
#include <haio_registry.hpp>

#include <iostream>
#include <string>
#include <vector>

using Haio::Color;
using Haio::Format;
using Haio::Image;

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (ok) return;
    std::cerr << "fail: " << what << '\n';
    failures++;
}

Image<Color::RGBA8888> solid(int width, int height, uint8_t red) {
    std::vector<uint8_t> data(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    for (size_t at = 0; at < data.size(); at += 4) {
        data[at] = red;
        data[at + 3] = 0xFF;
    }
    return Image<Color::RGBA8888>{width, height, std::move(data)};
}

/** the index member, which is the first one after the archive's magic */
std::string indexOf(const std::vector<uint8_t>& archive) {
    const std::string text(archive.begin(), archive.end());
    const auto header = text.find("0000000000.txt");
    if (header == std::string::npos) return {};

    const auto size = std::stoul(text.substr(header + 48, 10));
    return text.substr(header + 60, size);
}

std::vector<std::string> linesOf(const std::string& index) {
    std::vector<std::string> out;
    size_t at = 0;
    while (at < index.size()) {
        const auto end = index.find("\r\n", at);
        if (end == std::string::npos) break;
        out.push_back(index.substr(at, end - at));
        at = end + 2;
    }
    return out;
}

void testOne() {
    auto packed = Haio::Codecs::Pack<Format::ZCIS, Color::RGBA8888>({solid(32, 32, 0xFF)});
    check(packed.has_value(), "one picture packs");
    if (!packed) return;

    const auto lines = linesOf(indexOf(packed->data));
    check(lines.size() == 1, "one cut");
    if (lines.size() == 1) check(lines[0] == "0 0 32 32", "covering the whole canvas");
}

void testGrid() {
    std::vector<Image<Color::RGBA8888>> four;
    for (int at = 0; at < 4; at++) four.push_back(solid(32, 32, static_cast<uint8_t>(at * 60)));

    auto packed = Haio::Codecs::Pack<Format::ZCIS, Color::RGBA8888>(std::move(four));
    check(packed.has_value(), "four equal pictures pack");
    if (!packed) return;

    check(packed->format == Format::ZCIS && packed->contentType == "image/x-zcis", "and come out as zcis");

    const auto lines = linesOf(indexOf(packed->data));
    check(lines.size() == 4, "one cut each");
    if (lines.size() != 4) return;
    check(lines[0] == "0 0 32 32" && lines[1] == "32 0 32 32" && lines[2] == "0 32 32 32"
              && lines[3] == "32 32 32 32",
          "laid out as a two by two grid, in the order they were given");
}

/** the cell comes from the smallest, so a bigger one spans several of them */
void testMixed() {
    std::vector<Image<Color::RGBA8888>> mixed;
    mixed.push_back(solid(32, 32, 0xFF));
    mixed.push_back(solid(32, 32, 0x80));
    mixed.push_back(solid(16, 16, 0x40));

    auto packed = Haio::Codecs::Pack<Format::ZCIS, Color::RGBA8888>(std::move(mixed));
    check(packed.has_value(), "mixed sizes pack");
    if (!packed) return;

    const auto lines = linesOf(indexOf(packed->data));
    check(lines.size() == 3, "one cut each");
    if (lines.size() != 3) return;
    check(lines[0].ends_with("32 32") && lines[1].ends_with("32 32") && lines[2].ends_with("16 16"),
          "and every cut keeps its own size");
}

void testRefused() {
    check(!Haio::Codecs::Pack<Format::ZCIS, Color::RGBA8888>({}), "nothing to pack is refused");
    check(!Haio::Codecs::Pack<Format::ZCIS, Color::RGBA8888>({Image<Color::RGBA8888>{4, 4, {}}}),
          "and so is an image that does not match its own size");
}

/** only a container that can say where each picture is answers to -pack */
void testCapability() {
    check(Haio::packable(Format::ZCIS), "zcis can pack");
    check(!Haio::packable(Format::PNG), "png cannot");
    check(!Haio::packable(Format::JPEG), "jpeg cannot");
    check(!Haio::packable(Format::KTX2), "and neither can ktx2, which only holds one picture today");
}

}

auto main() -> int {
    testOne();
    testGrid();
    testMixed();
    testRefused();
    testCapability();

    if (failures == 0) std::cout << "pack zcis: ok\n";
    return failures == 0 ? 0 : 1;
}

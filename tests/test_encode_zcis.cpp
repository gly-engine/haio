#include <haio.hpp>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Member {
    std::string name;
    std::vector<uint8_t> body;
};

std::string trimField(const uint8_t* data, size_t size) {
    std::string out(reinterpret_cast<const char*>(data), size);
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

/** minimal ar reader, so the test checks the bytes we emit and not our own writer */
std::vector<Member> readArchive(const std::vector<uint8_t>& data) {
    assert(data.size() >= 8);
    assert(std::memcmp(data.data(), "!<arch>\n", 8) == 0);

    std::vector<Member> members;
    size_t offset = 8;
    while (offset < data.size()) {
        assert(offset + 60 <= data.size());
        assert(data[offset + 58] == '`' && data[offset + 59] == '\n');

        auto name = trimField(data.data() + offset, 16);
        if (!name.empty() && name.back() == '/') name.pop_back();
        const auto size = static_cast<size_t>(std::strtoul(trimField(data.data() + offset + 48, 10).c_str(), nullptr, 10));

        offset += 60;
        assert(offset + size <= data.size());
        members.push_back(Member{std::move(name), {data.begin() + static_cast<std::ptrdiff_t>(offset), data.begin() + static_cast<std::ptrdiff_t>(offset + size)}});

        offset += size;
        if (size % 2 != 0) {
            assert(offset < data.size() && data[offset] == '\n');
            offset++;
        }
    }
    return members;
}

struct Ppm {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgb;
};

Ppm readPpm(const std::vector<Member>::value_type& member) {
    const std::string text(reinterpret_cast<const char*>(member.body.data()), member.body.size());
    assert(text.starts_with("P6\n"));

    const auto space = text.find(' ', 3);
    const auto newline = text.find('\n', space);
    const auto headerEnd = text.find('\n', newline + 1);

    Ppm ppm;
    ppm.width = std::stoi(text.substr(3, space - 3));
    ppm.height = std::stoi(text.substr(space + 1, newline - space - 1));
    assert(text.substr(newline + 1, headerEnd - newline - 1) == "255");

    const auto start = headerEnd + 1;
    ppm.rgb.assign(member.body.begin() + static_cast<std::ptrdiff_t>(start), member.body.end());
    assert(ppm.rgb.size() == static_cast<size_t>(ppm.width) * static_cast<size_t>(ppm.height) * 3);
    return ppm;
}

Haio::Image<Haio::Color::RGBA8888> makeImage(int width, int height) {
    std::vector<uint8_t> data(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    for (size_t i = 0; i < data.size() / 4; i++) {
        data[i * 4 + 0] = static_cast<uint8_t>(i * 3 + 1);
        data[i * 4 + 1] = static_cast<uint8_t>(i * 5 + 2);
        data[i * 4 + 2] = static_cast<uint8_t>(i * 7 + 3);
        data[i * 4 + 3] = 255;
    }
    return Haio::Image<Haio::Color::RGBA8888>{width, height, std::move(data)};
}

void testTwoLayersAreLossless() {
    const auto source = makeImage(8, 4);
    const auto encoded = Haio::Codecs::Encode<Haio::Format::ZCIS>(source);
    assert(encoded);
    assert(encoded->format == Haio::Format::ZCIS);

    const auto members = readArchive(encoded->data);
    assert(members.size() == 3);

    assert(members[0].name == "000000000000.txt");
    assert(std::string(members[0].body.begin(), members[0].body.end()) == "0 0 8 4\r\n");

    // order, command, posx, posy, then the canvas size both layers scale up to
    assert(members[1].name == "1A00000804.ppm");
    assert(members[2].name == "2B00000804.ppm");

    const auto even = readPpm(members[1]);
    const auto odd = readPpm(members[2]);
    assert(even.width == 8 && even.height == 2);
    assert(odd.width == 8 && odd.height == 2);

    // interleaving the two layers must rebuild the source exactly
    for (int y = 0; y < source.height; y++) {
        const auto& layer = (y % 2 == 0) ? even : odd;
        for (int x = 0; x < source.width; x++) {
            const auto src = (static_cast<size_t>(y) * 8 + static_cast<size_t>(x)) * 4;
            const auto dst = (static_cast<size_t>(y / 2) * 8 + static_cast<size_t>(x)) * 3;
            assert(layer.rgb[dst + 0] == source.data[src + 0]);
            assert(layer.rgb[dst + 1] == source.data[src + 1]);
            assert(layer.rgb[dst + 2] == source.data[src + 2]);
        }
    }
}

void testSingleRowDropsTheRefineLayer() {
    const auto encoded = Haio::Codecs::Encode<Haio::Format::ZCIS>(makeImage(8, 1));
    assert(encoded);
    const auto members = readArchive(encoded->data);

    assert(members.size() == 2);
    assert(members[0].name == "000000000000.txt");
    assert(members[1].name == "1A00000801.ppm");
    assert(readPpm(members[1]).height == 1);
}

void testRejectsOversizedCanvas() {
    const auto encoded = Haio::Codecs::Encode<Haio::Format::ZCIS>(makeImage(3844, 2));
    assert(!encoded);
    assert(encoded.error().code == Haio::ErrorCode::InvalidInput);
}

void testFormatWiring() {
    assert(Haio::formatFromName("zcis") == Haio::Format::ZCIS);
    assert(Haio::formatFromExtension("photo.zcis") == Haio::Format::ZCIS);
    assert(Haio::formatName(Haio::Format::ZCIS) == "zcis");
    static_assert(Haio::Codecs::Encodable<Haio::Format::ZCIS, Haio::Color::RGBA8888>);
    static_assert(!Haio::Codecs::Decodable<Haio::Format::ZCIS, Haio::Color::RGBA8888>);
}

}

int main() {
    testTwoLayersAreLossless();
    testSingleRowDropsTheRefineLayer();
    testRejectsOversizedCanvas();
    testFormatWiring();
    return 0;
}

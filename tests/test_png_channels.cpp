#include <haio.hpp>

#include <cstdint>
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

void appendBE(std::vector<uint8_t>& out, uint32_t value) {
    for (int at = 3; at >= 0; at--) out.push_back(static_cast<uint8_t>(value >> (at * 8)));
}

uint32_t crc32Of(const std::vector<uint8_t>& data, size_t from) {
    static uint32_t table[256];
    static bool ready = false;
    if (!ready) {
        for (uint32_t n = 0; n < 256; n++) {
            uint32_t c = n;
            for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[n] = c;
        }
        ready = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t at = from; at < data.size(); at++) c = table[(c ^ data[at]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

/** stored deflate, so the test needs no compressor to write a png */
std::vector<uint8_t> storedZlib(const std::vector<uint8_t>& raw) {
    std::vector<uint8_t> out = {0x78, 0x01};
    out.push_back(0x01);
    out.push_back(static_cast<uint8_t>(raw.size() & 0xFF));
    out.push_back(static_cast<uint8_t>((raw.size() >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(~raw.size() & 0xFF));
    out.push_back(static_cast<uint8_t>((~raw.size() >> 8) & 0xFF));
    out.insert(out.end(), raw.begin(), raw.end());

    uint32_t a = 1;
    uint32_t b = 0;
    for (const auto byte : raw) {
        a = (a + byte) % 65521;
        b = (b + a) % 65521;
    }
    appendBE(out, (b << 16) | a);
    return out;
}

void appendChunk(std::vector<uint8_t>& png, const char* kind, const std::vector<uint8_t>& body) {
    appendBE(png, static_cast<uint32_t>(body.size()));
    const auto from = png.size();
    for (int at = 0; at < 4; at++) png.push_back(static_cast<uint8_t>(kind[at]));
    png.insert(png.end(), body.begin(), body.end());
    appendBE(png, crc32Of(png, from));
}

/** colourType 2 is rgb with no alpha, which is the one that took the wrong branch */
std::vector<uint8_t> makePng(uint8_t colourType, const std::vector<uint8_t>& row) {
    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};

    const auto pixelSize = colourType == 2 ? 3u : 4u;
    std::vector<uint8_t> header;
    appendBE(header, static_cast<uint32_t>(row.size() / pixelSize));
    appendBE(header, 1);
    header.insert(header.end(), {8, colourType, 0, 0, 0});
    appendChunk(png, "IHDR", header);

    std::vector<uint8_t> raw = {0};   // filter none
    raw.insert(raw.end(), row.begin(), row.end());
    appendChunk(png, "IDAT", storedZlib(raw));
    appendChunk(png, "IEND", {});
    return png;
}

}

auto main() -> int {
    /**
     * red first, blue second.
     *
     * wuffs offers both channel orders under names that differ by three letters, so
     * asking for the wrong one hands back a picture that is entirely valid and has
     * red and blue swapped. nothing downstream can tell: every stage after it copies
     * three bytes faithfully.
     */
    for (const auto colourType : {uint8_t{2}, uint8_t{6}}) {
        const auto row = colourType == 2
            ? std::vector<uint8_t>{255, 0, 0, 0, 0, 255}
            : std::vector<uint8_t>{255, 0, 0, 255, 0, 0, 255, 255};

        const auto png = makePng(colourType, row);
        const auto name = "colour type " + std::to_string(colourType);

        const auto found = Haio::Detect(png);
        check(found.format == Haio::Format::PNG, name + " is detected as png");

        const auto decoded = Haio::Decode(Haio::Blob{Haio::Format::RAW, Haio::Color::RGBA8888, {}, {}, png});
        check(decoded.has_value(), name + " decodes");
        if (!decoded) continue;

        check(decoded->width == 2 && decoded->height == 1, name + " keeps its size");
        check(decoded->data[0] == 255 && decoded->data[1] == 0 && decoded->data[2] == 0,
              name + ": the first pixel is red, not blue");
        check(decoded->data[4] == 0 && decoded->data[5] == 0 && decoded->data[6] == 255,
              name + ": the second pixel is blue, not red");
    }

    if (failures == 0) std::cout << "png channels: ok\n";
    return failures == 0 ? 0 : 1;
}

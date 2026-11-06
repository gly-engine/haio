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

void little16(std::vector<uint8_t>& out, size_t at, int value) {
    out[at] = static_cast<uint8_t>(value & 0xFF);
    out[at + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

/**
 * a pcx header written out by hand, so a test says what it is testing rather than
 * calling the encoder and comparing it with itself.
 */
std::vector<uint8_t> header(int width, int height, int bits, int planes, int bytesPerLine,
                            int encoding = 1, int version = 5) {
    std::vector<uint8_t> out(128, 0);
    out[0] = 0x0A;
    out[1] = static_cast<uint8_t>(version);
    out[2] = static_cast<uint8_t>(encoding);
    out[3] = static_cast<uint8_t>(bits);
    little16(out, 8, width - 1);
    little16(out, 10, height - 1);
    out[65] = static_cast<uint8_t>(planes);
    little16(out, 66, bytesPerLine);
    little16(out, 68, 1);
    return out;
}

/** the run length the format uses, for writing fixtures by hand */
void packRow(std::vector<uint8_t>& out, const std::vector<uint8_t>& row) {
    size_t at = 0;
    while (at < row.size()) {
        const auto value = row[at];
        size_t run = 1;
        while (at + run < row.size() && row[at + run] == value && run < 63) run++;
        if (run > 1 || value >= 0xC0) {
            out.push_back(static_cast<uint8_t>(0xC0 | run));
            out.push_back(value);
        } else {
            out.push_back(value);
        }
        at += run;
    }
}

Blob blobOf(std::vector<uint8_t> data) {
    return Blob{Format::RAW, Color::RGBA8888, {}, {}, std::move(data)};
}

void testRejectsNonsense() {
    check(!Codecs::Detect<Format::PCX, Color::PALETTE>(Bytes{}), "nothing is not a pcx");

    auto wrongMagic = header(2, 2, 8, 1, 2);
    wrongMagic[0] = 0x0B;
    check(!Codecs::Detect<Format::PCX, Color::PALETTE>(wrongMagic), "the magic byte is the first thing checked");

    // five bits a pixel never existed, and a reader that believes it reads garbage
    check(!Codecs::Detect<Format::PCX, Color::PALETTE>(header(2, 2, 5, 1, 2)), "five bits a pixel is not a depth");
    check(!Codecs::Detect<Format::PCX, Color::PALETTE>(header(2, 2, 8, 9, 2)), "nine planes is not a pcx");

    // a row that cannot hold the pixels the header claims is the file disagreeing
    // with itself, which is the only thing a one byte magic has to go on
    check(!Codecs::Detect<Format::PCX, Color::PALETTE>(header(64, 2, 8, 1, 4)), "a row too short for its own width");
}

/** the two numbers that name the colour are read together, never one alone */
void testColorOfDepth() {
    check(Codecs::Detect<Format::PCX, Color::PALETTE>(header(4, 2, 8, 1, 4)), "eight bits in one plane indexes");
    check(Codecs::Detect<Format::PCX, Color::PALETTE>(header(8, 2, 1, 4, 2)), "one bit across four planes indexes");
    check(Codecs::Detect<Format::PCX, Color::PALETTE>(header(8, 2, 1, 1, 2)), "one bit in one plane indexes");
    check(Codecs::Detect<Format::PCX, Color::RGB888>(header(4, 2, 8, 3, 4)), "eight bits across three planes is colour");
    check(!Codecs::Detect<Format::PCX, Color::PALETTE>(header(4, 2, 8, 3, 4)), "a colour pcx does not index");
    check(!Codecs::Detect<Format::PCX, Color::RGB888>(header(4, 2, 8, 1, 4)), "an indexed pcx is not colour");
}

/**
 * four one bit planes folded back into one index, with plane zero holding the low
 * bit. it is the layout ega hardware wanted and the one place a pcx decoder is
 * most often wrong -- imagemagick agrees with this fixture, pillow does not.
 */
void testPlanarEga() {
    auto data = header(8, 1, 1, 4, 2);
    for (int index = 0; index < 16; index++) {
        data[16 + index * 3 + 0] = static_cast<uint8_t>(index * 16);
        data[16 + index * 3 + 1] = static_cast<uint8_t>(index * 16);
        data[16 + index * 3 + 2] = static_cast<uint8_t>(index * 16);
    }

    const std::vector<uint8_t> wanted{0, 1, 2, 3, 4, 5, 6, 7};
    for (int plane = 0; plane < 4; plane++) {
        uint8_t byte = 0;
        for (size_t x = 0; x < wanted.size(); x++) {
            if ((wanted[x] >> plane) & 1) byte |= static_cast<uint8_t>(1u << (7 - x));
        }
        packRow(data, {byte, 0});
    }

    const auto out = Codecs::Decode<Format::PCX, Color::PALETTE>(blobOf(data));
    check(out.has_value(), "a planar ega pcx decodes");
    if (!out) return;
    check(out->width == 8 && out->height == 1, "the size comes from the window, not the row");
    check(out->data == wanted, "plane zero holds the low bit of the index");
    check(out->entries.size() == 16, "four one bit planes index sixteen colours");
    check(out->entries[3] == 0xFF303030u, "the sixteen colours come from the header");
}

/**
 * most mono pcx files leave the header palette zeroed, which would make a picture
 * of one colour twice. black and white is what they meant.
 */
void testMonochromeDefaultsToBlackAndWhite() {
    auto data = header(8, 1, 1, 1, 2);
    packRow(data, {0b10101010, 0});

    const auto out = Codecs::Decode<Format::PCX, Color::PALETTE>(blobOf(data));
    check(out.has_value(), "a one bit pcx decodes");
    if (!out) return;
    check(out->entries.size() == 2, "one bit indexes two colours");
    check(out->entries[0] == 0xFF000000u && out->entries[1] == 0xFFFFFFFFu,
          "an empty header palette means black and white");
    const std::vector<uint8_t> wanted{1, 0, 1, 0, 1, 0, 1, 0};
    check(out->data == wanted, "the leftmost pixel is the top bit");
}

void testRefusesTruncatedAndOverlongRuns() {
    auto cut = header(8, 2, 8, 1, 8);
    packRow(cut, std::vector<uint8_t>(8, 7));
    check(!Codecs::Decode<Format::PCX, Color::PALETTE>(blobOf(cut)), "a file that stops after one row is truncated");

    auto spill = header(4, 1, 8, 1, 4);
    spill.push_back(0xC0 | 60);  // sixty of something, into a row that holds four
    spill.push_back(9);
    check(!Codecs::Decode<Format::PCX, Color::PALETTE>(blobOf(spill)), "a run past the last row is refused");

    auto zero = header(4, 1, 8, 1, 4);
    zero.push_back(0xC0);  // a run of nothing, which would never end
    zero.push_back(9);
    check(!Codecs::Decode<Format::PCX, Color::PALETTE>(blobOf(zero)), "a run of zero is refused");
}

/** an index with no colour behind it is a file to refuse, not a pixel to guess at */
void testRefusesIndexPastThePalette() {
    auto data = header(2, 1, 1, 1, 2);
    packRow(data, {0b11000000, 0});
    auto image = Codecs::Decode<Format::PCX, Color::PALETTE>(blobOf(data));
    check(image.has_value(), "two colours is enough for two one bit pixels");

    Image<Color::PALETTE> bad{2, 1, {0, 5}};
    bad.entries = {0xFF000000u, 0xFFFFFFFFu};
    check(!Codecs::Encode<Format::PCX, Color::PALETTE>(bad, Settings{}), "encoding refuses an index with no colour");
}

/** what goes in comes out, including the colours and the ones nobody used */
void testPaletteRoundTrip() {
    Image<Color::PALETTE> img{5, 3, {}};
    img.data = {0, 1, 2, 3, 4,
                4, 3, 2, 1, 0,
                1, 1, 1, 1, 1};
    img.entries = {0xFF000000u, 0xFFFF0000u, 0xFF00FF00u, 0xFF0000FFu, 0xFFFFFFFFu};

    const auto blob = Codecs::Encode<Format::PCX, Color::PALETTE>(img, Settings{});
    check(blob.has_value(), "a palette image encodes");
    if (!blob) return;
    check(blob->format == Format::PCX && blob->color == Color::PALETTE, "the blob says what it is");
    check(Codecs::Detect<Format::PCX, Color::PALETTE>(blob->data), "what haio writes, haio recognises");

    const auto back = Codecs::Decode<Format::PCX, Color::PALETTE>(*blob);
    check(back.has_value(), "and reads back");
    if (!back) return;
    check(back->width == 5 && back->height == 3, "the odd width survives the even row");
    check(back->data == img.data, "every index survives");
    check(back->entries.size() == 256, "the trailer always holds 256 colours");
    for (size_t at = 0; at < img.entries.size(); at++) {
        check(back->entries[at] == img.entries[at], "colour " + std::to_string(at) + " survives");
    }
}

/**
 * a literal above 0xbf cannot be written as itself, because the top two bits are
 * what marks a count. it is the one byte value that costs two, and the one a
 * round trip is most likely to lose.
 */
void testHighBytesSurviveTheRunLength() {
    Image<Color::PALETTE> img{4, 1, {0xFF, 0xC0, 0xBF, 0xFF}};
    img.entries.assign(256, 0xFF000000u);

    const auto blob = Codecs::Encode<Format::PCX, Color::PALETTE>(img, Settings{});
    check(blob.has_value(), "high indices encode");
    if (!blob) return;
    const auto back = Codecs::Decode<Format::PCX, Color::PALETTE>(*blob);
    check(back.has_value() && back->data == img.data, "an index above 0xbf survives the run length");
}

void testTruecolorRoundTrip() {
    Image<Color::RGB888> img{3, 2, {}};
    img.data = {255, 0, 0,   0, 255, 0,   0, 0, 255,
                16, 32, 48,  200, 100, 50, 255, 255, 255};

    const auto blob = Codecs::Encode<Format::PCX, Color::RGB888>(img, Settings{});
    check(blob.has_value(), "a colour image encodes");
    if (!blob) return;
    check(Codecs::Detect<Format::PCX, Color::RGB888>(blob->data), "and is recognised as colour");

    const auto back = Codecs::Decode<Format::PCX, Color::RGB888>(*blob);
    check(back.has_value(), "and reads back");
    if (!back) return;
    check(back->width == 3 && back->height == 2, "the size survives");
    check(back->data == img.data, "every channel survives the planes");
}

/** encoding 0 is rare and legal: the pixels are simply there, a row at a time */
void testUncompressed() {
    auto data = header(4, 2, 8, 1, 4, 0);
    data.insert(data.end(), {1, 2, 3, 4, 5, 6, 7, 8});

    const auto out = Codecs::Decode<Format::PCX, Color::PALETTE>(blobOf(data));
    check(out.has_value(), "an uncompressed pcx decodes");
    if (!out) return;
    const std::vector<uint8_t> wanted{1, 2, 3, 4, 5, 6, 7, 8};
    check(out->data == wanted, "with the pixels where they lie");
}

/**
 * the rows as they are, which the format allows and which beats the run length on
 * noise: a byte above 0xbf costs two when packed, so a picture that never repeats
 * comes out larger compressed than plain.
 */
void testCompressionChoice() {
    Image<Color::PALETTE> img{4, 2, {1, 1, 1, 1, 2, 2, 2, 2}};
    img.entries = {0xFF000000u, 0xFFFF0000u, 0xFF00FF00u};

    const auto packed = Codecs::Encode<Format::PCX, Color::PALETTE>(img, Settings{});
    const auto plain = Codecs::Encode<Format::PCX, Color::PALETTE>(
        img, Settings{Setting{"pcx:compression", "none"}});
    check(packed.has_value() && plain.has_value(), "both ways encode");
    if (!packed || !plain) return;

    check(packed->data[2] == 1, "the header says the rows are packed");
    check(plain->data[2] == 0, "and says when they are not");
    check(plain->data.size() == 128 + 8 + 769, "plain rows are the pixels and nothing else");
    check(packed->data.size() < plain->data.size(), "four of a kind is worth packing");

    const auto fromPacked = Codecs::Decode<Format::PCX, Color::PALETTE>(*packed);
    const auto fromPlain = Codecs::Decode<Format::PCX, Color::PALETTE>(*plain);
    check(fromPacked.has_value() && fromPlain.has_value(), "both ways decode");
    if (!fromPacked || !fromPlain) return;
    check(fromPacked->data == img.data && fromPlain->data == img.data, "and give back the same pixels");
}

}

int main() {
    testRejectsNonsense();
    testColorOfDepth();
    testPlanarEga();
    testMonochromeDefaultsToBlackAndWhite();
    testRefusesTruncatedAndOverlongRuns();
    testRefusesIndexPastThePalette();
    testPaletteRoundTrip();
    testHighBytesSurviveTheRunLength();
    testTruecolorRoundTrip();
    testUncompressed();
    testCompressionChoice();

    if (failures == 0) std::cout << "ok\n";
    return failures == 0 ? 0 : 1;
}

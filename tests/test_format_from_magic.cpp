#include <haio.hpp>

#include <array>
#include <cassert>
#include <string_view>
#include <vector>

namespace {

std::vector<uint8_t> bytes(std::string_view text) {
    return {text.begin(), text.end()};
}

void testEachDetectorMatchesOnlyItself() {
    const std::vector<std::pair<Haio::Format, std::vector<uint8_t>>> samples = {
        {Haio::Format::PNG, {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'}},
        {Haio::Format::KTX, {0xab, 'K', 'T', 'X', ' ', '1', '1', 0xbb, '\r', '\n', 0x1a, '\n'}},
        {Haio::Format::KTX2, {0xab, 'K', 'T', 'X', ' ', '2', '0', 0xbb, '\r', '\n', 0x1a, '\n'}},
        {Haio::Format::DDS, {'D', 'D', 'S', ' ', 124, 0, 0, 0}},
        {Haio::Format::PVR, {'P', 'V', 'R', 0x03}},
        {Haio::Format::PPM, {'P', '6', '\n'}},
        {Haio::Format::ZCIS, {'!', '<', 'a', 'r', 'c', 'h', '>', '\n'}},
    };

    for (const auto& [format, data] : samples) {
        assert(Haio::formatFromMagic(data) == format);
    }
}

void testDetectIsPerFormat() {
    const auto png = std::vector<uint8_t>{0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    assert(Haio::Detect<Haio::Format::PNG>(png));
    assert(!Haio::Detect<Haio::Format::KTX>(png));
    assert(!Haio::Detect<Haio::Format::ZCIS>(png));

    // headerless payloads carry nothing to recognise
    assert(!Haio::Detect<Haio::Format::ETC1>(png));
    assert(!Haio::Detect<Haio::Format::RGB565>(png));
}

void testKtxVersionsDoNotCollide() {
    const auto ktx = std::vector<uint8_t>{0xab, 'K', 'T', 'X', ' ', '1', '1', 0xbb, '\r', '\n', 0x1a, '\n'};
    const auto ktx2 = std::vector<uint8_t>{0xab, 'K', 'T', 'X', ' ', '2', '0', 0xbb, '\r', '\n', 0x1a, '\n'};

    assert(Haio::Detect<Haio::Format::KTX>(ktx) && !Haio::Detect<Haio::Format::KTX2>(ktx));
    assert(Haio::Detect<Haio::Format::KTX2>(ktx2) && !Haio::Detect<Haio::Format::KTX>(ktx2));
}

void testDdsNeedsItsHeaderSize() {
    // the four cc alone must not be enough
    assert(Haio::formatFromMagic(bytes("DDS \x01\x00\x00\x00")) == Haio::Format::RAW);
    assert(Haio::formatFromMagic(bytes("DDS ")) == Haio::Format::RAW);
}

void testTruncatedInputIsNotAMatch() {
    assert(Haio::formatFromMagic({}) == Haio::Format::RAW);
    assert(Haio::formatFromMagic(std::vector<uint8_t>{0x89, 'P'}) == Haio::Format::RAW);
    assert(Haio::formatFromMagic(bytes("!<arc")) == Haio::Format::RAW);
}

void testForeignFormatsAreNamed() {
    assert(Haio::describeForeignMagic(std::vector<uint8_t>{0xff, 0xd8, 0xff, 0xe0}) == "jpeg");
    assert(Haio::describeForeignMagic(bytes("GIF89a....")) == "gif");
    assert(Haio::describeForeignMagic(bytes("RIFF____WEBPVP8 ")) == "webp");
    assert(Haio::describeForeignMagic(bytes("BM______")) == "bmp");

    assert(Haio::describeForeignMagic(bytes("not a picture")).empty());
    // anything haio can read is not foreign
    assert(Haio::describeForeignMagic(std::vector<uint8_t>{0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'}).empty());
}

void testContentTypeMapping() {
    assert(Haio::formatFromContentType("image/png") == Haio::Format::PNG);
    assert(Haio::formatFromContentType("image/png; charset=binary") == Haio::Format::PNG);
    assert(Haio::formatFromContentType("  IMAGE/PNG  ") == Haio::Format::PNG);
    assert(Haio::formatFromContentType("image/x-zcis") == Haio::Format::ZCIS);

    assert(Haio::formatFromContentType("image/jpeg") == Haio::Format::RAW);
    assert(Haio::formatFromContentType("application/octet-stream") == Haio::Format::RAW);
    assert(Haio::formatFromContentType({}) == Haio::Format::RAW);
}

void testBytesOutrankTheServerAndTheUrl() {
    const auto png = std::vector<uint8_t>{0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};

    // a gam creative: no extension in the url, content type is the only hint
    assert(Haio::detectFormat(png, "image/png", "/creative_9931") == Haio::Format::PNG);
    // the server lies, the bytes do not
    assert(Haio::detectFormat(png, "image/jpeg", "/x.dds") == Haio::Format::PNG);
    // nothing recognisable in the bytes, so the content type decides
    assert(Haio::detectFormat(bytes("????"), "image/png", "/x") == Haio::Format::PNG);
    // and the extension is the last resort
    assert(Haio::detectFormat(bytes("????"), "application/octet-stream", "/x.ktx") == Haio::Format::KTX);
    // an unknown extension must not throw, the way formatFromExtension does
    assert(Haio::detectFormat(bytes("????"), {}, "/x.tar.gz") == Haio::Format::RAW);
    assert(Haio::detectFormat({}, {}, {}) == Haio::Format::RAW);
}

}

int main() {
    testEachDetectorMatchesOnlyItself();
    testDetectIsPerFormat();
    testKtxVersionsDoNotCollide();
    testDdsNeedsItsHeaderSize();
    testTruncatedInputIsNotAMatch();
    testForeignFormatsAreNamed();
    testContentTypeMapping();
    testBytesOutrankTheServerAndTheUrl();
    return 0;
}

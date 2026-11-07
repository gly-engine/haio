#include <haio.hpp>

#include <array>
#include <cassert>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::vector<uint8_t> bytes(std::string_view text) {
    return {text.begin(), text.end()};
}

/** an ar member header: name, then mtime, uid, gid, mode, size and the terminator */
std::string member(std::string_view name, std::string_view body) {
    std::string header(60, ' ');
    header.replace(0, name.size(), name);
    const auto size = std::to_string(body.size());
    header.replace(48, size.size(), size);
    header.replace(58, 2, "`\n");
    return header + std::string(body) + (body.size() % 2 ? "\n" : "");
}

void testEachDetectorMatchesOnlyItself() {
    struct Sample { Haio::Format format; Haio::Color color; std::vector<uint8_t> data; };
    const std::vector<Sample> samples = {
        {Haio::Format::PNG,  Haio::Color::RGBA8888, {0x89,'P','N','G','\r','\n',0x1a,'\n',0,0,0,13,'I','H','D','R',0,0,0,8,0,0,0,8,8,6}},
        {Haio::Format::PPM,  Haio::Color::RGB888,   {'P','6','\n'}},
        {Haio::Format::ZCIS, Haio::Color::RGBA8888, bytes("!<arch>\n" + member("0000000000.txt", "0 0 8 8\r\n"))},
        // riff, then "WEBP", then the chunk that says which bitstream: vp8l is lossless
        {Haio::Format::WEBP, Haio::Color::RGBA8888,
         {'R','I','F','F',20,0,0,0,'W','E','B','P','V','P','8','L',13,0,0,0,0x2f,0,0,0,0}},
    };

    // a codec switched off with -DHAIO_CODEC_<NAME>=OFF stops recognising its own bytes
    for (const auto& sample : samples) {
        const auto found = Haio::Detect(sample.data);
        if (Haio::detectable(sample.format, sample.color)) {
            assert(found.format == sample.format && found.color == sample.color);
        } else {
            assert(!found);
        }
    }
}

void testDetectIsPerFormat() {
    // headerless colours declare no Detect at all, so asking is a compile error
    static_assert(!Haio::Codecs::Detectable<Haio::Format::RAW, Haio::Color::ETC1>);
    static_assert(Haio::Codecs::Detectable<Haio::Format::PNG, Haio::Color::RGBA8888>);
    // recognised with no decoder behind it
    static_assert(Haio::Codecs::Detectable<Haio::Format::PNG, Haio::Color::GRAY8>);
    static_assert(!Haio::Codecs::Decodable<Haio::Format::PNG, Haio::Color::GRAY8>);
}



void testZcisNeedsTheIndexFirst() {
    assert(Haio::Detect(bytes("!<arch>\n" + member("0000000000.txt", "0 0 1 1\r\n") + member("1A00000101.ppm", "x")))
               .format == Haio::Format::ZCIS);
    // the index anywhere but first is not zcis, nor is an ar without it
    assert(Haio::Detect(bytes("!<arch>\n" + member("notes.txt", "odd") + member("0000000000.txt", "0 0 1 1\r\n"))).format
           != Haio::Format::ZCIS);
    assert(Haio::Detect(bytes("!<arch>\n" + member("notes.txt", "hello"))).format != Haio::Format::ZCIS);
    assert(Haio::Detect(bytes("!<arch>\n")).format != Haio::Format::ZCIS);
}

void testTruncatedInputIsNotAMatch() {
    assert(!Haio::Detect({}));
    assert(!Haio::Detect(std::vector<uint8_t>{0x89, 'P'}));
    assert(!Haio::Detect(bytes("!<arc")));
}

/**
 * jpeg used to be named by a hand written magic table beside the registry. it is a
 * codec now, so the registry names it like any other, and a format with no detector
 * is simply not recognised rather than named by a second mechanism.
 */
void testJpegIsACodecLikeAnyOther() {
    const auto found = Haio::Detect(std::vector<uint8_t>{0xff, 0xd8, 0xff, 0xe0});
    assert(found);
    assert(found.format == Haio::Format::JPEG);
    assert(found.color == Haio::Color::YUV420);

    // a jpeg is yuv, and that is the only pair it has: asking for any other is a
    // compile error rather than a silent conversion inside the codec
    static_assert(Haio::Codecs::Detectable<Haio::Format::JPEG, Haio::Color::YUV420>);
    static_assert(Haio::Codecs::Decodable<Haio::Format::JPEG, Haio::Color::YUV420>);
    static_assert(Haio::Codecs::Encodable<Haio::Format::JPEG, Haio::Color::YUV420>);
    static_assert(!Haio::Codecs::Decodable<Haio::Format::JPEG, Haio::Color::RGBA8888>);
    static_assert(!Haio::Codecs::Encodable<Haio::Format::JPEG, Haio::Color::RGBA8888>);

    // and yuv reaches the rest of haio by converting, where anybody can see it happen
    static_assert(Haio::Codecs::Convertible<Haio::Color::YUV420, Haio::Color::RGBA8888>);
    static_assert(Haio::Codecs::Convertible<Haio::Color::RGBA8888, Haio::Color::YUV420>);

    // four bytes of magic is a jpeg to Detect and nothing to a decoder
    assert(!Haio::Decode(Haio::Blob{Haio::Format::RAW, Haio::Color::RGBA8888, {}, {},
                                    {0xff, 0xd8, 0xff, 0xe0}}));

    const auto gif = Haio::Detect(bytes("GIF89a...."));
    assert(gif);
    assert(gif.format == Haio::Format::GIF);
    assert(gif.color == Haio::Color::RGBA8888);

    // formats haio has no detector for are simply unrecognised
    assert(!Haio::Detect(bytes("BM______")));
}

/** the file name names a container, and a container may answer to several spellings */
void testExtensionsResolveThroughTheirAliases() {
    assert(Haio::formatFromExtension("a.jpg") == Haio::Format::JPEG);
    assert(Haio::formatFromExtension("a.jpeg") == Haio::Format::JPEG);
    assert(Haio::formatFromExtension("a.pgm") == Haio::Format::PPM);
    assert(Haio::formatFromExtension("a.pbm") == Haio::Format::PPM);
    assert(Haio::formatFromExtension("a.PNG") == Haio::Format::PNG);
    assert(Haio::formatFromExtension("a.xyz") == Haio::Format::RAW);
    assert(Haio::formatFromExtension("noextension") == Haio::Format::RAW);

    // the first spelling is the one haio writes, and it round trips
    for (const auto format : {Haio::Format::JPEG, Haio::Format::PPM, Haio::Format::PNG}) {
        const auto ext = Haio::extensionFor(format);
        assert(Haio::formatFromExtension("a." + std::string(ext)) == format);
    }
}



}

int main() {
    testEachDetectorMatchesOnlyItself();
    testDetectIsPerFormat();
    testZcisNeedsTheIndexFirst();
    testTruncatedInputIsNotAMatch();
    testJpegIsACodecLikeAnyOther();
    testExtensionsResolveThroughTheirAliases();
    return 0;
}

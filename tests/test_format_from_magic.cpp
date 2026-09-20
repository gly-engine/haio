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
    struct Sample { Haio::Format format; Haio::Color color; std::vector<uint8_t> data; };
    const std::vector<Sample> samples = {
        {Haio::Format::PNG,  Haio::Color::RGBA8888, {0x89,'P','N','G','\r','\n',0x1a,'\n',0,0,0,13,'I','H','D','R',0,0,0,8,0,0,0,8,8,6}},
        {Haio::Format::PPM,  Haio::Color::RGB888,   {'P','6','\n'}},
        {Haio::Format::ZCIS, Haio::Color::RGBA8888, {'!','<','a','r','c','h','>','\n'}},
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
void testJpegIsRecognisedWithoutADecoder() {
    const auto found = Haio::Detect(std::vector<uint8_t>{0xff, 0xd8, 0xff, 0xe0});
    assert(found);
    assert(found.format == Haio::Format::JPEG);
    assert(found.color == Haio::Color::YUV420);

    // detected, and honest about not being able to open it
    static_assert(Haio::Codecs::Detectable<Haio::Format::JPEG, Haio::Color::YUV420>);
    static_assert(!Haio::Codecs::Decodable<Haio::Format::JPEG, Haio::Color::YUV420>);
    assert(!Haio::Decode(Haio::Blob{Haio::Format::RAW, Haio::Color::RGBA8888, {}, {},
                                    {0xff, 0xd8, 0xff, 0xe0}}));

    // formats haio has no detector for are simply unrecognised
    assert(!Haio::Detect(bytes("GIF89a....")));
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
    testTruncatedInputIsNotAMatch();
    testJpegIsRecognisedWithoutADecoder();
    testExtensionsResolveThroughTheirAliases();
    return 0;
}

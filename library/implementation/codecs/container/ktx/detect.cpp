#include <haio/generated/codecs/classes.hpp>
#include <haio/implementation/codecs.hpp>
#include <haio/util.hpp>

#include <haio/util.hpp>

namespace {

/** the container is only half the answer: the header also says which colour it holds */
bool isContainer(Haio::Implementation::Bytes data) {


    constexpr std::array<uint8_t, 12> magic = {0xab, 'K', 'T', 'X', ' ', '1', '1', 0xbb, '\r', '\n', 0x1a, '\n'};
    return Haio::Util::matches(data, magic);
}

}

namespace Haio::Implementation::Codecs {

bool Detect<Format::KTX, Color::ETC1>::operator()(Bytes data) const {
    if (!isContainer(data)) return false;
    const auto colour = Util::getGpuColorFromGL(Util::readU32LE(data, 16), Util::readU32LE(data, 24), Util::readU32LE(data, 28));
    return colour && *colour == Color::ETC1;
}

bool Detect<Format::KTX, Color::RGB565>::operator()(Bytes data) const {
    if (!isContainer(data)) return false;
    const auto colour = Util::getGpuColorFromGL(Util::readU32LE(data, 16), Util::readU32LE(data, 24), Util::readU32LE(data, 28));
    return colour && *colour == Color::RGB565;
}

bool Detect<Format::KTX, Color::RGBA8888>::operator()(Bytes data) const {
    if (!isContainer(data)) return false;
    const auto colour = Util::getGpuColorFromGL(Util::readU32LE(data, 16), Util::readU32LE(data, 24), Util::readU32LE(data, 28));
    return colour && *colour == Color::RGBA8888;
}
}

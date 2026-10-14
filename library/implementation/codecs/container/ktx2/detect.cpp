#include <haio/generated/codecs/classes.hpp>
#include <haio/implementation/codecs.hpp>
#include <haio/util.hpp>

#include <haio/util.hpp>

namespace {

/** the container is only half the answer: the header also says which colour it holds */
bool isContainer(Haio::Implementation::Bytes data) {


    constexpr std::array<uint8_t, 12> magic = {0xab, 'K', 'T', 'X', ' ', '2', '0', 0xbb, '\r', '\n', 0x1a, '\n'};
    return Haio::Util::matches(data, magic);
}

}

namespace Haio::Implementation::Codecs {

bool Detect<Format::KTX2, Color::ETC1>::operator()(Bytes data) const {
    if (!isContainer(data)) return false;
    const auto colour = Util::getGpuColorFromVK(Util::readU32LE(data, 12));
    return colour && *colour == Color::ETC1;
}

bool Detect<Format::KTX2, Color::RGB565>::operator()(Bytes data) const {
    if (!isContainer(data)) return false;
    const auto colour = Util::getGpuColorFromVK(Util::readU32LE(data, 12));
    return colour && *colour == Color::RGB565;
}

bool Detect<Format::KTX2, Color::RGBA8888>::operator()(Bytes data) const {
    if (!isContainer(data)) return false;
    const auto colour = Util::getGpuColorFromVK(Util::readU32LE(data, 12));
    return colour && *colour == Color::RGBA8888;
}
}

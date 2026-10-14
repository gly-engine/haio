#include <haio/generated/codecs/classes.hpp>
#include <haio/implementation/codecs.hpp>
#include <haio/util.hpp>

#include <haio/util.hpp>

namespace {

/** the container is only half the answer: the header also says which colour it holds */
bool isContainer(Haio::Implementation::Bytes data) {


    constexpr std::array<uint8_t, 4> magic = {'P', 'V', 'R', 0x03};
    return Haio::Util::matches(data, magic);
}

}

namespace Haio::Implementation::Codecs {

bool Detect<Format::PVR, Color::ETC1>::operator()(Bytes data) const {
    if (!isContainer(data)) return false;
    const auto colour = Util::getGpuColorFromPVR(Util::readU64LE(data, 8));
    return colour && *colour == Color::ETC1;
}

bool Detect<Format::PVR, Color::RGB565>::operator()(Bytes data) const {
    if (!isContainer(data)) return false;
    const auto colour = Util::getGpuColorFromPVR(Util::readU64LE(data, 8));
    return colour && *colour == Color::RGB565;
}

bool Detect<Format::PVR, Color::RGBA8888>::operator()(Bytes data) const {
    if (!isContainer(data)) return false;
    const auto colour = Util::getGpuColorFromPVR(Util::readU64LE(data, 8));
    return colour && *colour == Color::RGBA8888;
}
}

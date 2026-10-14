#include <haio/generated/codecs/classes.hpp>
#include <haio/implementation/codecs.hpp>
#include <haio/implementation/codecs/convert.hpp>
#include <haio/util.hpp>

namespace Haio::Implementation::Codecs {
namespace {

/** one body for every colour the container can hold; the caller names which */
template <Color P>
Result<Image<P>> decodeKtx(const Blob& blob) {
    if (blob.data.size() < 68) HAIO_FAIL(InvalidInput, "invalid ktx header");
    if (Util::readU32LE(blob.data, 12) != 0x04030201) HAIO_FAIL(InvalidInput, "unsupported ktx endianness");
    if (Util::readU32LE(blob.data, 44) || Util::readU32LE(blob.data, 48)
        || Util::readU32LE(blob.data, 52) != 1 || Util::readU32LE(blob.data, 56) > 1) {
        HAIO_FAIL(InvalidInput, "unsupported ktx texture shape");
    }

    const Size size{static_cast<int>(Util::readU32LE(blob.data, 36)), static_cast<int>(Util::readU32LE(blob.data, 40))};
    const auto dataOff = static_cast<size_t>(64) + Util::readU32LE(blob.data, 60);
    if (!Util::hasBytes(blob.data, dataOff, 4)) HAIO_FAIL(InvalidInput, "truncated ktx container");

    HAIO_TRY(data, Util::slice(blob.data, dataOff + 4, Util::readU32LE(blob.data, dataOff)));

    HAIO_TRY(expected, sizeOf(P, size));
    if (data.size() != expected) HAIO_FAIL(InvalidInput, "invalid ktx payload size");

    return Image<P>{size.width, size.height, std::move(data)};
}

}

CodecRes<Color::ETC1> Decode<Format::KTX, Color::ETC1>::operator()(CodecIn blob) const {
    return decodeKtx<Color::ETC1>(blob);
}

CodecRes<Color::RGB565> Decode<Format::KTX, Color::RGB565>::operator()(CodecIn blob) const {
    return decodeKtx<Color::RGB565>(blob);
}

CodecRes<Color::RGBA8888> Decode<Format::KTX, Color::RGBA8888>::operator()(CodecIn blob) const {
    return decodeKtx<Color::RGBA8888>(blob);
}
}

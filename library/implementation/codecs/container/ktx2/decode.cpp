#include <haio/generated/codecs/classes.hpp>
#include <haio/implementation/codecs.hpp>
#include <haio/implementation/codecs/convert.hpp>
#include <haio/util.hpp>

namespace Haio::Implementation::Codecs {
namespace {

/** one body for every colour the container can hold; the caller names which */
template <Color P>
Result<Image<P>> decodeKtx2(const Blob& blob) {
    if (blob.data.size() < 104) HAIO_FAIL(InvalidInput, "invalid ktx2 header");
    if (Util::readU32LE(blob.data, 28) || Util::readU32LE(blob.data, 32) || Util::readU32LE(blob.data, 36) != 1
        || Util::readU32LE(blob.data, 40) != 1 || Util::readU32LE(blob.data, 44)) {
        HAIO_FAIL(InvalidInput, "unsupported ktx2 texture shape");
    }

    const Size size{static_cast<int>(Util::readU32LE(blob.data, 20)), static_cast<int>(Util::readU32LE(blob.data, 24))};
    HAIO_TRY(data, Util::slice(blob.data, static_cast<size_t>(Util::readU64LE(blob.data, 80)),
                                          static_cast<size_t>(Util::readU64LE(blob.data, 88))));

    HAIO_TRY(expected, sizeOf(P, size));
    if (data.size() != expected) HAIO_FAIL(InvalidInput, "invalid ktx2 payload size");

    return Image<P>{size.width, size.height, std::move(data)};
}

}

CodecRes<Color::ETC1> Decode<Format::KTX2, Color::ETC1>::operator()(CodecIn blob) const {
    return decodeKtx2<Color::ETC1>(blob);
}

CodecRes<Color::RGB565> Decode<Format::KTX2, Color::RGB565>::operator()(CodecIn blob) const {
    return decodeKtx2<Color::RGB565>(blob);
}

CodecRes<Color::RGBA8888> Decode<Format::KTX2, Color::RGBA8888>::operator()(CodecIn blob) const {
    return decodeKtx2<Color::RGBA8888>(blob);
}
}

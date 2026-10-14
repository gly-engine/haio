#include <haio/generated/codecs/classes.hpp>
#include <haio/implementation/codecs.hpp>
#include <haio/implementation/codecs/convert.hpp>
#include <haio/util.hpp>

namespace Haio::Implementation::Codecs {
namespace {

/** one body for every colour the container can hold; the caller names which */
template <Color P>
Result<Image<P>> decodePvr(const Blob& blob) {
    if (blob.data.size() < 52) HAIO_FAIL(InvalidInput, "invalid pvr header");
    if (Util::readU32LE(blob.data, 32) != 1 || Util::readU32LE(blob.data, 36) != 1
        || Util::readU32LE(blob.data, 40) != 1 || Util::readU32LE(blob.data, 44) != 1) {
        HAIO_FAIL(InvalidInput, "unsupported pvr texture shape");
    }

    const Size size{static_cast<int>(Util::readU32LE(blob.data, 28)), static_cast<int>(Util::readU32LE(blob.data, 24))};
    HAIO_TRY(want, sizeOf(P, size));
    HAIO_TRY(data, Util::slice(blob.data, static_cast<size_t>(52) + Util::readU32LE(blob.data, 48), want));

    HAIO_TRY(expected, sizeOf(P, size));
    if (data.size() != expected) HAIO_FAIL(InvalidInput, "invalid pvr payload size");

    return Image<P>{size.width, size.height, std::move(data)};
}

}

CodecRes<Color::ETC1> Decode<Format::PVR, Color::ETC1>::operator()(CodecIn blob) const {
    return decodePvr<Color::ETC1>(blob);
}

CodecRes<Color::RGB565> Decode<Format::PVR, Color::RGB565>::operator()(CodecIn blob) const {
    return decodePvr<Color::RGB565>(blob);
}

CodecRes<Color::RGBA8888> Decode<Format::PVR, Color::RGBA8888>::operator()(CodecIn blob) const {
    return decodePvr<Color::RGBA8888>(blob);
}
}

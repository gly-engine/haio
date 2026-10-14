#include <haio/generated/codecs/classes.hpp>
#include <haio/implementation/codecs.hpp>
#include <haio/implementation/codecs/convert.hpp>
#include <haio/util.hpp>

namespace Haio::Implementation::Codecs {
namespace {

/** one body for every colour the container can hold; the caller names which */
template <Color P>
Result<Image<P>> decodeDds(const Blob& blob) {
    if (blob.data.size() < 128 || Util::readU32LE(blob.data, 4) != 124 || Util::readU32LE(blob.data, 76) != 32) {
        HAIO_FAIL(InvalidInput, "invalid dds header");
    }

    const Size size{static_cast<int>(Util::readU32LE(blob.data, 16)), static_cast<int>(Util::readU32LE(blob.data, 12))};
    HAIO_TRY(want, sizeOf(P, size));
    HAIO_TRY(data, Util::slice(blob.data, 128, want));

    HAIO_TRY(expected, sizeOf(P, size));
    if (data.size() != expected) HAIO_FAIL(InvalidInput, "invalid dds payload size");

    return Image<P>{size.width, size.height, std::move(data)};
}

}

CodecRes<Color::RGB565> Decode<Format::DDS, Color::RGB565>::operator()(CodecIn blob) const {
    return decodeDds<Color::RGB565>(blob);
}

CodecRes<Color::RGBA8888> Decode<Format::DDS, Color::RGBA8888>::operator()(CodecIn blob) const {
    return decodeDds<Color::RGBA8888>(blob);
}
}

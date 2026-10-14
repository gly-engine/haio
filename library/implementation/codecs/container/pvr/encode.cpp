#include <haio/generated/codecs/classes.hpp>
#include <haio/implementation/codecs.hpp>
#include <haio/implementation/codecs/convert.hpp>
#include <haio/util.hpp>

namespace Haio::Implementation::Codecs {
namespace {

template <Color P>
Result<Blob> encodePvr(Image<P> img) {
    const Size size{img.width, img.height};
    HAIO_TRY(expected, sizeOf(P, size));
    if (img.data.size() != expected) HAIO_FAIL(InvalidInput, "pvr payload does not match its size");

    HAIO_TRY(profile, Util::getGpuProfile(P, size));
    std::vector<uint8_t> out;
    for (auto v : {0x03525650u, 0u}) Util::appendU32LE(out, v);
    Util::appendU64LE(out, profile.pvrFormat);
    for (auto v : {0u, 0u, static_cast<uint32_t>(size.height), static_cast<uint32_t>(size.width),
                   1u, 1u, 1u, 1u, 0u}) {
        Util::appendU32LE(out, v);
    }
    out.insert(out.end(), img.data.begin(), img.data.end());

    return Blob{Format::PVR, P, "image/x-pvr", {}, std::move(out)};
}

}

CodecOut Encode<Format::PVR, Color::ETC1>::operator()(Image<Color::ETC1> img) const {
    return encodePvr<Color::ETC1>(std::move(img));
}

CodecOut Encode<Format::PVR, Color::RGB565>::operator()(Image<Color::RGB565> img) const {
    return encodePvr<Color::RGB565>(std::move(img));
}

CodecOut Encode<Format::PVR, Color::RGBA8888>::operator()(Image<Color::RGBA8888> img) const {
    return encodePvr<Color::RGBA8888>(std::move(img));
}
}

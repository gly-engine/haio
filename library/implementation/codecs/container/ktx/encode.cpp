#include <haio/generated/codecs/classes.hpp>
#include <haio/implementation/codecs.hpp>
#include <haio/implementation/codecs/convert.hpp>
#include <haio/util.hpp>

namespace Haio::Implementation::Codecs {
namespace {

template <Color P>
Result<Blob> encodeKtx(Image<P> img) {
    const Size size{img.width, img.height};
    HAIO_TRY(expected, sizeOf(P, size));
    if (img.data.size() != expected) HAIO_FAIL(InvalidInput, "ktx payload does not match its size");

    HAIO_TRY(profile, Util::getGpuProfile(P, size));
    std::vector<uint8_t> out = {0xab, 0x4b, 0x54, 0x58, 0x20, 0x31, 0x31, 0xbb, 0x0d, 0x0a, 0x1a, 0x0a};
    for (auto v : {0x04030201u, profile.glType, Util::getGpuTypeSize(profile), profile.glFormat, profile.glInternal,
                   profile.glBase, static_cast<uint32_t>(size.width), static_cast<uint32_t>(size.height),
                   0u, 0u, 1u, 1u, 0u}) {
        Util::appendU32LE(out, v);
    }
    Util::appendU32LE(out, static_cast<uint32_t>(img.data.size()));
    out.insert(out.end(), img.data.begin(), img.data.end());
    while (out.size() % 4) out.push_back(0);

    return Blob{Format::KTX, P, "image/ktx", {}, std::move(out)};
}

}

CodecOut Encode<Format::KTX, Color::ETC1>::operator()(Image<Color::ETC1> img) const {
    return encodeKtx<Color::ETC1>(std::move(img));
}

CodecOut Encode<Format::KTX, Color::RGB565>::operator()(Image<Color::RGB565> img) const {
    return encodeKtx<Color::RGB565>(std::move(img));
}

CodecOut Encode<Format::KTX, Color::RGBA8888>::operator()(Image<Color::RGBA8888> img) const {
    return encodeKtx<Color::RGBA8888>(std::move(img));
}
}

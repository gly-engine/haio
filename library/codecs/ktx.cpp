#include "gpu_container_common.hpp"

namespace Haio {

template <>
Stage Encode<Format::KTX>() {
    return [](const Image& img) {
        GpuContainer::validatePayload(img);
        const auto p = GpuContainer::profileFor(img.type, img.width, img.height);
        std::vector<uint8_t> out = {0xab, 0x4b, 0x54, 0x58, 0x20, 0x31, 0x31, 0xbb, 0x0d, 0x0a, 0x1a, 0x0a};
        for (auto v : {0x04030201u, p.glType, GpuContainer::typeSize(p), p.glFormat, p.glInternal, p.glBase, static_cast<uint32_t>(img.width),
                       static_cast<uint32_t>(img.height), 0u, 0u, 1u, 1u, 0u}) {
            GpuContainer::appendU32(out, v);
        }
        GpuContainer::appendU32(out, static_cast<uint32_t>(img.data.size()));
        out.insert(out.end(), img.data.begin(), img.data.end());
        while (out.size() % 4) out.push_back(0);
        return GpuContainer::wrapped(Format::KTX, img.width, img.height, std::move(out));
    };
}

template <>
Stage Decode<Format::KTX>() {
    return [](const Image& img) {
        static constexpr std::array<uint8_t, 12> id = {0xab, 0x4b, 0x54, 0x58, 0x20, 0x31, 0x31, 0xbb, 0x0d, 0x0a, 0x1a, 0x0a};
        if (img.data.size() < 68 || !std::equal(id.begin(), id.end(), img.data.begin())) throw std::runtime_error("invalid ktx header");
        if (GpuContainer::readU32(img.data, 12) != 0x04030201) throw std::runtime_error("unsupported ktx endianness");
        const auto format = GpuContainer::ktxFormat(GpuContainer::readU32(img.data, 16), GpuContainer::readU32(img.data, 24), GpuContainer::readU32(img.data, 28));
        const auto width = static_cast<int>(GpuContainer::readU32(img.data, 36));
        const auto height = static_cast<int>(GpuContainer::readU32(img.data, 40));
        if (GpuContainer::readU32(img.data, 44) || GpuContainer::readU32(img.data, 48) || GpuContainer::readU32(img.data, 52) != 1 || GpuContainer::readU32(img.data, 56) > 1) {
            throw std::runtime_error("unsupported ktx texture shape");
        }
        const auto dataOff = static_cast<size_t>(64) + GpuContainer::readU32(img.data, 60);
        const auto imageSize = GpuContainer::readU32(img.data, dataOff);
        auto data = GpuContainer::slice(img.data, dataOff + 4, imageSize);
        if (data.size() != GpuContainer::payloadSize(format, width, height)) throw std::runtime_error("invalid ktx payload size");
        return GpuContainer::wrapped(format, width, height, std::move(data));
    };
}

}

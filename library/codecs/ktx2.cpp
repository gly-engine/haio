#include "gpu_container_common.hpp"

namespace Haio {

template <>
Stage Encode<Format::KTX2>() {
    return [](const Image& img) {
        GpuContainer::validatePayload(img);
        const auto p = GpuContainer::profileFor(img.type, img.width, img.height);
        std::vector<uint8_t> out = {0xab, 0x4b, 0x54, 0x58, 0x20, 0x32, 0x30, 0xbb, 0x0d, 0x0a, 0x1a, 0x0a};
        for (auto v : {p.vkFormat, GpuContainer::typeSize(p), static_cast<uint32_t>(img.width),
                       static_cast<uint32_t>(img.height), 0u, 0u, 1u, 1u, 0u, 0u, 0u, 0u, 0u}) GpuContainer::appendU32(out, v);
        GpuContainer::appendU64(out, 0);
        GpuContainer::appendU64(out, 0);
        GpuContainer::appendU64(out, 104);
        GpuContainer::appendU64(out, img.data.size());
        GpuContainer::appendU64(out, img.data.size());
        out.insert(out.end(), img.data.begin(), img.data.end());
        return GpuContainer::wrapped(Format::KTX2, img.width, img.height, std::move(out));
    };
}

template <>
Stage Decode<Format::KTX2>() {
    return [](const Image& img) {
        static constexpr std::array<uint8_t, 12> id = {0xab, 0x4b, 0x54, 0x58, 0x20, 0x32, 0x30, 0xbb, 0x0d, 0x0a, 0x1a, 0x0a};
        if (img.data.size() < 104 || !std::equal(id.begin(), id.end(), img.data.begin())) throw std::runtime_error("invalid ktx2 header");
        const auto format = GpuContainer::vkFormat(GpuContainer::readU32(img.data, 12));
        const auto width = static_cast<int>(GpuContainer::readU32(img.data, 20));
        const auto height = static_cast<int>(GpuContainer::readU32(img.data, 24));
        if (GpuContainer::readU32(img.data, 28) || GpuContainer::readU32(img.data, 32) || GpuContainer::readU32(img.data, 36) != 1 || GpuContainer::readU32(img.data, 40) != 1 || GpuContainer::readU32(img.data, 44)) {
            throw std::runtime_error("unsupported ktx2 texture shape");
        }
        const auto off = static_cast<size_t>(GpuContainer::readU64(img.data, 80));
        const auto len = static_cast<size_t>(GpuContainer::readU64(img.data, 88));
        auto data = GpuContainer::slice(img.data, off, len);
        if (data.size() != GpuContainer::payloadSize(format, width, height)) throw std::runtime_error("invalid ktx2 payload size");
        return GpuContainer::wrapped(format, width, height, std::move(data));
    };
}

}

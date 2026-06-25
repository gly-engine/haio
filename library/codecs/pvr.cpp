#include "gpu_container_common.hpp"

namespace Haio {

template <>
Stage Encode<Format::PVR>() {
    return [](const Image& img) {
        GpuContainer::validatePayload(img);
        const auto p = GpuContainer::profileFor(img.type, img.width, img.height);
        std::vector<uint8_t> out;
        for (auto v : {0x03525650u, 0u}) GpuContainer::appendU32(out, v);
        GpuContainer::appendU64(out, p.pvrFormat);
        for (auto v : {0u, 0u, static_cast<uint32_t>(img.height), static_cast<uint32_t>(img.width), 1u, 1u, 1u, 1u, 0u}) GpuContainer::appendU32(out, v);
        out.insert(out.end(), img.data.begin(), img.data.end());
        return GpuContainer::wrapped(Format::PVR, img.width, img.height, std::move(out));
    };
}

template <>
Stage Decode<Format::PVR>() {
    return [](const Image& img) {
        if (img.data.size() < 52 || GpuContainer::readU32(img.data, 0) != 0x03525650) throw std::runtime_error("invalid pvr header");
        const auto format = GpuContainer::pvrFormat(GpuContainer::readU64(img.data, 8));
        const auto height = static_cast<int>(GpuContainer::readU32(img.data, 24));
        const auto width = static_cast<int>(GpuContainer::readU32(img.data, 28));
        if (GpuContainer::readU32(img.data, 32) != 1 || GpuContainer::readU32(img.data, 36) != 1 || GpuContainer::readU32(img.data, 40) != 1 || GpuContainer::readU32(img.data, 44) != 1) {
            throw std::runtime_error("unsupported pvr texture shape");
        }
        const auto off = static_cast<size_t>(52) + GpuContainer::readU32(img.data, 48);
        auto data = GpuContainer::slice(img.data, off, GpuContainer::payloadSize(format, width, height));
        return GpuContainer::wrapped(format, width, height, std::move(data));
    };
}

}

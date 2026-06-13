#include "gpu_container_common.hpp"

namespace Haio {

template <>
Stage Encode<Format::DDS>() {
    return [](const Image& img) {
        GpuContainer::validatePayload(img, false);
        const auto pitch = static_cast<uint32_t>(img.width * (img.type == Format::RGB565 ? 2 : 4));
        const auto flags = img.type == Format::RGB565 ? 0x40u : 0x41u;
        const auto bits = img.type == Format::RGB565 ? 16u : 32u;
        const auto r = img.type == Format::RGB565 ? 0xf800u : 0x000000ffu;
        const auto g = img.type == Format::RGB565 ? 0x07e0u : 0x0000ff00u;
        const auto b = img.type == Format::RGB565 ? 0x001fu : 0x00ff0000u;
        const auto a = img.type == Format::RGB565 ? 0u : 0xff000000u;

        std::vector<uint8_t> out;
        for (auto v : {GpuContainer::fourcc('D', 'D', 'S', ' '), 124u, 0x100fu, static_cast<uint32_t>(img.height), static_cast<uint32_t>(img.width),
                       pitch, 0u, 0u}) GpuContainer::appendU32(out, v);
        for (int i = 0; i < 11; i++) GpuContainer::appendU32(out, 0);
        for (auto v : {32u, flags, 0u, bits, r, g, b, a, 0x1000u, 0u, 0u, 0u, 0u}) GpuContainer::appendU32(out, v);
        out.insert(out.end(), img.data.begin(), img.data.end());
        return GpuContainer::wrapped(Format::DDS, img.width, img.height, std::move(out));
    };
}

template <>
Stage Decode<Format::DDS>() {
    return [](const Image& img) {
        if (img.data.size() < 128 || GpuContainer::readU32(img.data, 0) != GpuContainer::fourcc('D', 'D', 'S', ' ') || GpuContainer::readU32(img.data, 4) != 124 || GpuContainer::readU32(img.data, 76) != 32) {
            throw std::runtime_error("invalid dds header");
        }
        const auto height = static_cast<int>(GpuContainer::readU32(img.data, 12));
        const auto width = static_cast<int>(GpuContainer::readU32(img.data, 16));
        const auto bits = GpuContainer::readU32(img.data, 88);
        const auto r = GpuContainer::readU32(img.data, 92);
        const auto g = GpuContainer::readU32(img.data, 96);
        const auto b = GpuContainer::readU32(img.data, 100);
        const auto a = GpuContainer::readU32(img.data, 104);
        Format format;
        if (bits == 16 && r == 0xf800 && g == 0x07e0 && b == 0x001f && a == 0) {
            format = Format::RGB565;
        } else if (bits == 32 && r == 0x000000ff && g == 0x0000ff00 && b == 0x00ff0000 && a == 0xff000000) {
            format = Format::RGBA8888;
        } else {
            throw std::runtime_error("unsupported dds payload format");
        }
        auto data = GpuContainer::slice(img.data, 128, GpuContainer::payloadSize(format, width, height));
        return GpuContainer::wrapped(format, width, height, std::move(data));
    };
}

}

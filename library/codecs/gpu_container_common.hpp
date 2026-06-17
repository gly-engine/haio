#pragma once

#include <haio.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace Haio::GpuContainer {

static constexpr uint32_t GL_UNSIGNED_BYTE = 0x1401;
static constexpr uint32_t GL_UNSIGNED_SHORT_5_6_5 = 0x8363;
static constexpr uint32_t GL_RGB = 0x1907;
static constexpr uint32_t GL_RGBA = 0x1908;
static constexpr uint32_t GL_RGBA8 = 0x8058;
static constexpr uint32_t GL_RGB565 = 0x8d62;
static constexpr uint32_t GL_ETC1_RGB8_OES = 0x8d64;
static constexpr uint32_t VK_FORMAT_R5G6B5_UNORM_PACK16 = 4;
static constexpr uint32_t VK_FORMAT_R8G8B8A8_UNORM = 37;
static constexpr uint32_t VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK = 147;

struct Profile {
    uint32_t glType;
    uint32_t glFormat;
    uint32_t glInternal;
    uint32_t glBase;
    uint32_t vkFormat;
    uint64_t pvrFormat;
    size_t size;
};

static inline uint32_t fourcc(char a, char b, char c, char d) {
    return static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8) | (static_cast<uint32_t>(c) << 16) | (static_cast<uint32_t>(d) << 24);
}

static inline uint64_t pvrRaw(char a, char b, char c, char d, uint8_t ba, uint8_t bb, uint8_t bc, uint8_t bd) {
    return static_cast<uint64_t>(a) | (static_cast<uint64_t>(b) << 8) | (static_cast<uint64_t>(c) << 16) | (static_cast<uint64_t>(d) << 24)
         | (static_cast<uint64_t>(ba) << 32) | (static_cast<uint64_t>(bb) << 40) | (static_cast<uint64_t>(bc) << 48) | (static_cast<uint64_t>(bd) << 56);
}

static inline size_t etc1Size(int width, int height) {
    return static_cast<size_t>((width + 3) / 4) * static_cast<size_t>((height + 3) / 4) * 8;
}

static inline size_t payloadSize(Format format, int width, int height) {
    if (width <= 0 || height <= 0) throw std::runtime_error("gpu container requires positive dimensions");
    if (format == Format::ETC1) return etc1Size(width, height);
    if (format == Format::RGB565) return static_cast<size_t>(width) * static_cast<size_t>(height) * 2;
    if (format == Format::RGBA8888) return static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    throw std::runtime_error("unsupported gpu payload format");
}

static inline Profile profileFor(Format format, int width, int height) {
    const auto size = payloadSize(format, width, height);
    if (format == Format::ETC1) return {0, 0, GL_ETC1_RGB8_OES, GL_RGB, VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK, 6, size};
    if (format == Format::RGB565) return {GL_UNSIGNED_SHORT_5_6_5, GL_RGB, GL_RGB565, GL_RGB, VK_FORMAT_R5G6B5_UNORM_PACK16, pvrRaw('r', 'g', 'b', 0, 5, 6, 5, 0), size};
    return {GL_UNSIGNED_BYTE, GL_RGBA, GL_RGBA8, GL_RGBA, VK_FORMAT_R8G8B8A8_UNORM, pvrRaw('r', 'g', 'b', 'a', 8, 8, 8, 8), size};
}

static inline uint32_t typeSize(const Profile& p) {
    return p.glType == GL_UNSIGNED_SHORT_5_6_5 ? 2u : 1u;
}

static inline void validatePayload(const Image& img, bool allowEtc1 = true) {
    if (!allowEtc1 && img.type == Format::ETC1) {
        throw std::runtime_error("dds does not support etc1 payloads");
    }
    const auto p = profileFor(img.type, img.width, img.height);
    if (img.data.size() != p.size) throw std::runtime_error("invalid gpu payload size");
}

static inline void appendU32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v));
    out.push_back(static_cast<uint8_t>(v >> 8));
    out.push_back(static_cast<uint8_t>(v >> 16));
    out.push_back(static_cast<uint8_t>(v >> 24));
}

static inline void appendU64(std::vector<uint8_t>& out, uint64_t v) {
    appendU32(out, static_cast<uint32_t>(v));
    appendU32(out, static_cast<uint32_t>(v >> 32));
}

static inline uint32_t readU32(const std::vector<uint8_t>& data, size_t off) {
    if (off + 4 > data.size()) throw std::runtime_error("truncated gpu container");
    return static_cast<uint32_t>(data[off + 0]) | (static_cast<uint32_t>(data[off + 1]) << 8)
         | (static_cast<uint32_t>(data[off + 2]) << 16) | (static_cast<uint32_t>(data[off + 3]) << 24);
}

static inline uint64_t readU64(const std::vector<uint8_t>& data, size_t off) {
    return static_cast<uint64_t>(readU32(data, off)) | (static_cast<uint64_t>(readU32(data, off + 4)) << 32);
}

static inline std::vector<uint8_t> slice(const std::vector<uint8_t>& data, size_t off, size_t len) {
    if (off > data.size() || len > data.size() - off) throw std::runtime_error("truncated gpu payload");
    return {data.begin() + static_cast<std::ptrdiff_t>(off), data.begin() + static_cast<std::ptrdiff_t>(off + len)};
}

static inline Image wrapped(Format type, int width, int height, std::vector<uint8_t> data) {
    return Image{type, width, height, std::move(data)};
}

static inline Format ktxFormat(uint32_t glType, uint32_t glFormat, uint32_t glInternal) {
    if (glType == 0 && glFormat == 0 && glInternal == GL_ETC1_RGB8_OES) return Format::ETC1;
    if (glType == GL_UNSIGNED_SHORT_5_6_5 && glFormat == GL_RGB && glInternal == GL_RGB565) return Format::RGB565;
    if (glType == GL_UNSIGNED_BYTE && glFormat == GL_RGBA && glInternal == GL_RGBA8) return Format::RGBA8888;
    throw std::runtime_error("unsupported ktx payload format");
}

static inline Format vkFormat(uint32_t format) {
    if (format == VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK) return Format::ETC1;
    if (format == VK_FORMAT_R5G6B5_UNORM_PACK16) return Format::RGB565;
    if (format == VK_FORMAT_R8G8B8A8_UNORM) return Format::RGBA8888;
    throw std::runtime_error("unsupported ktx2 payload format");
}

static inline Format pvrFormat(uint64_t format) {
    if (format == 6) return Format::ETC1;
    if (format == pvrRaw('r', 'g', 'b', 0, 5, 6, 5, 0)) return Format::RGB565;
    if (format == pvrRaw('r', 'g', 'b', 'a', 8, 8, 8, 8)) return Format::RGBA8888;
    throw std::runtime_error("unsupported pvr payload format");
}

}

#include <haio.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace Haio {

namespace {

constexpr uint32_t GL_UNSIGNED_BYTE = 0x1401;
constexpr uint32_t GL_UNSIGNED_SHORT_5_6_5 = 0x8363;
constexpr uint32_t GL_RGB = 0x1907;
constexpr uint32_t GL_RGBA = 0x1908;
constexpr uint32_t GL_RGBA8 = 0x8058;
constexpr uint32_t GL_RGB565 = 0x8d62;
constexpr uint32_t GL_ETC1_RGB8_OES = 0x8d64;
constexpr uint32_t VK_R5G6B5 = 4;
constexpr uint32_t VK_R8G8B8A8 = 37;
constexpr uint32_t VK_ETC2_RGB = 147;

struct Profile {
    uint32_t glType;
    uint32_t glFormat;
    uint32_t glInternal;
    uint32_t glBase;
    uint32_t vkFormat;
    uint64_t pvrFormat;
    size_t size;
};

uint32_t fourcc(char a, char b, char c, char d) {
    return static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8) | (static_cast<uint32_t>(c) << 16) | (static_cast<uint32_t>(d) << 24);
}

uint64_t pvrRaw(char a, char b, char c, char d, uint8_t ba, uint8_t bb, uint8_t bc, uint8_t bd) {
    return static_cast<uint64_t>(a) | (static_cast<uint64_t>(b) << 8) | (static_cast<uint64_t>(c) << 16) | (static_cast<uint64_t>(d) << 24)
         | (static_cast<uint64_t>(ba) << 32) | (static_cast<uint64_t>(bb) << 40) | (static_cast<uint64_t>(bc) << 48) | (static_cast<uint64_t>(bd) << 56);
}

size_t etc1Size(int width, int height) {
    return static_cast<size_t>((width + 3) / 4) * static_cast<size_t>((height + 3) / 4) * 8;
}

size_t payloadSize(Format format, int width, int height) {
    if (width <= 0 || height <= 0) throw std::runtime_error("gpu container requires positive dimensions");
    if (format == Format::ETC1) return etc1Size(width, height);
    if (format == Format::RGB565) return static_cast<size_t>(width) * static_cast<size_t>(height) * 2;
    if (format == Format::RGBA8888) return static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    throw std::runtime_error("unsupported gpu payload format");
}

Profile profileFor(Format format, int width, int height) {
    const auto size = payloadSize(format, width, height);
    if (format == Format::ETC1) return {0, 0, GL_ETC1_RGB8_OES, GL_RGB, VK_ETC2_RGB, 6, size};
    if (format == Format::RGB565) return {GL_UNSIGNED_SHORT_5_6_5, GL_RGB, GL_RGB565, GL_RGB, VK_R5G6B5, pvrRaw('r', 'g', 'b', 0, 5, 6, 5, 0), size};
    return {GL_UNSIGNED_BYTE, GL_RGBA, GL_RGBA8, GL_RGBA, VK_R8G8B8A8, pvrRaw('r', 'g', 'b', 'a', 8, 8, 8, 8), size};
}

uint32_t typeSize(const Profile& p) {
    return p.glType == GL_UNSIGNED_SHORT_5_6_5 ? 2u : 1u;
}

void validatePayload(const Image& img, bool allowEtc1 = true) {
    if (!allowEtc1 && img.type == Format::ETC1) {
        throw std::runtime_error("dds does not support etc1 payloads");
    }
    const auto p = profileFor(img.type, img.width, img.height);
    if (img.data.size() != p.size) throw std::runtime_error("invalid gpu payload size");
}

void appendU32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v));
    out.push_back(static_cast<uint8_t>(v >> 8));
    out.push_back(static_cast<uint8_t>(v >> 16));
    out.push_back(static_cast<uint8_t>(v >> 24));
}

void appendU64(std::vector<uint8_t>& out, uint64_t v) {
    appendU32(out, static_cast<uint32_t>(v));
    appendU32(out, static_cast<uint32_t>(v >> 32));
}

uint32_t readU32(const std::vector<uint8_t>& data, size_t off) {
    if (off + 4 > data.size()) throw std::runtime_error("truncated gpu container");
    return static_cast<uint32_t>(data[off + 0]) | (static_cast<uint32_t>(data[off + 1]) << 8)
         | (static_cast<uint32_t>(data[off + 2]) << 16) | (static_cast<uint32_t>(data[off + 3]) << 24);
}

uint64_t readU64(const std::vector<uint8_t>& data, size_t off) {
    return static_cast<uint64_t>(readU32(data, off)) | (static_cast<uint64_t>(readU32(data, off + 4)) << 32);
}

std::vector<uint8_t> slice(const std::vector<uint8_t>& data, size_t off, size_t len) {
    if (off > data.size() || len > data.size() - off) throw std::runtime_error("truncated gpu payload");
    return {data.begin() + static_cast<std::ptrdiff_t>(off), data.begin() + static_cast<std::ptrdiff_t>(off + len)};
}

Image wrapped(Format type, int width, int height, std::vector<uint8_t> data) {
    return Image{type, width, height, std::move(data)};
}

Format ktxFormat(uint32_t glType, uint32_t glFormat, uint32_t glInternal) {
    if (glType == 0 && glFormat == 0 && glInternal == GL_ETC1_RGB8_OES) return Format::ETC1;
    if (glType == GL_UNSIGNED_SHORT_5_6_5 && glFormat == GL_RGB && glInternal == GL_RGB565) return Format::RGB565;
    if (glType == GL_UNSIGNED_BYTE && glFormat == GL_RGBA && glInternal == GL_RGBA8) return Format::RGBA8888;
    throw std::runtime_error("unsupported ktx payload format");
}

Format vkFormat(uint32_t format) {
    if (format == VK_ETC2_RGB) return Format::ETC1;
    if (format == VK_R5G6B5) return Format::RGB565;
    if (format == VK_R8G8B8A8) return Format::RGBA8888;
    throw std::runtime_error("unsupported ktx2 payload format");
}

Format pvrFormat(uint64_t format) {
    if (format == 6) return Format::ETC1;
    if (format == pvrRaw('r', 'g', 'b', 0, 5, 6, 5, 0)) return Format::RGB565;
    if (format == pvrRaw('r', 'g', 'b', 'a', 8, 8, 8, 8)) return Format::RGBA8888;
    throw std::runtime_error("unsupported pvr payload format");
}

}

template <>
Stage Encode<Format::KTX>() {
    return [](const Image& img) {
        validatePayload(img);
        const auto p = profileFor(img.type, img.width, img.height);
        std::vector<uint8_t> out = {0xab, 0x4b, 0x54, 0x58, 0x20, 0x31, 0x31, 0xbb, 0x0d, 0x0a, 0x1a, 0x0a};
        for (auto v : {0x04030201u, p.glType, typeSize(p), p.glFormat, p.glInternal, p.glBase, static_cast<uint32_t>(img.width),
                       static_cast<uint32_t>(img.height), 0u, 0u, 1u, 1u, 0u}) {
            appendU32(out, v);
        }
        appendU32(out, static_cast<uint32_t>(img.data.size()));
        out.insert(out.end(), img.data.begin(), img.data.end());
        while (out.size() % 4) out.push_back(0);
        return wrapped(Format::KTX, img.width, img.height, std::move(out));
    };
}

template <>
Stage Decode<Format::KTX>() {
    return [](const Image& img) {
        static constexpr std::array<uint8_t, 12> id = {0xab, 0x4b, 0x54, 0x58, 0x20, 0x31, 0x31, 0xbb, 0x0d, 0x0a, 0x1a, 0x0a};
        if (img.data.size() < 68 || !std::equal(id.begin(), id.end(), img.data.begin())) throw std::runtime_error("invalid ktx header");
        if (readU32(img.data, 12) != 0x04030201) throw std::runtime_error("unsupported ktx endianness");
        const auto format = ktxFormat(readU32(img.data, 16), readU32(img.data, 24), readU32(img.data, 28));
        const auto width = static_cast<int>(readU32(img.data, 36));
        const auto height = static_cast<int>(readU32(img.data, 40));
        if (readU32(img.data, 44) || readU32(img.data, 48) || readU32(img.data, 52) != 1 || readU32(img.data, 56) > 1) {
            throw std::runtime_error("unsupported ktx texture shape");
        }
        const auto dataOff = static_cast<size_t>(64 + readU32(img.data, 60));
        const auto imageSize = readU32(img.data, dataOff);
        auto data = slice(img.data, dataOff + 4, imageSize);
        if (data.size() != payloadSize(format, width, height)) throw std::runtime_error("invalid ktx payload size");
        return wrapped(format, width, height, std::move(data));
    };
}

template <>
Stage Encode<Format::PVR>() {
    return [](const Image& img) {
        validatePayload(img);
        const auto p = profileFor(img.type, img.width, img.height);
        std::vector<uint8_t> out;
        for (auto v : {0x03525650u, 0u}) appendU32(out, v);
        appendU64(out, p.pvrFormat);
        for (auto v : {0u, 0u, static_cast<uint32_t>(img.height), static_cast<uint32_t>(img.width), 1u, 1u, 1u, 1u, 0u}) appendU32(out, v);
        out.insert(out.end(), img.data.begin(), img.data.end());
        return wrapped(Format::PVR, img.width, img.height, std::move(out));
    };
}

template <>
Stage Decode<Format::PVR>() {
    return [](const Image& img) {
        if (img.data.size() < 52 || readU32(img.data, 0) != 0x03525650) throw std::runtime_error("invalid pvr header");
        const auto format = pvrFormat(readU64(img.data, 8));
        const auto height = static_cast<int>(readU32(img.data, 24));
        const auto width = static_cast<int>(readU32(img.data, 28));
        if (readU32(img.data, 32) != 1 || readU32(img.data, 36) != 1 || readU32(img.data, 40) != 1 || readU32(img.data, 44) != 1) {
            throw std::runtime_error("unsupported pvr texture shape");
        }
        const auto off = static_cast<size_t>(52 + readU32(img.data, 48));
        auto data = slice(img.data, off, payloadSize(format, width, height));
        return wrapped(format, width, height, std::move(data));
    };
}

template <>
Stage Encode<Format::DDS>() {
    return [](const Image& img) {
        validatePayload(img, false);
        const auto pitch = static_cast<uint32_t>(img.width * (img.type == Format::RGB565 ? 2 : 4));
        const auto flags = img.type == Format::RGB565 ? 0x40u : 0x41u;
        const auto bits = img.type == Format::RGB565 ? 16u : 32u;
        const auto r = img.type == Format::RGB565 ? 0xf800u : 0x000000ffu;
        const auto g = img.type == Format::RGB565 ? 0x07e0u : 0x0000ff00u;
        const auto b = img.type == Format::RGB565 ? 0x001fu : 0x00ff0000u;
        const auto a = img.type == Format::RGB565 ? 0u : 0xff000000u;

        std::vector<uint8_t> out;
        for (auto v : {fourcc('D', 'D', 'S', ' '), 124u, 0x100fu, static_cast<uint32_t>(img.height), static_cast<uint32_t>(img.width),
                       pitch, 0u, 0u}) appendU32(out, v);
        for (int i = 0; i < 11; i++) appendU32(out, 0);
        for (auto v : {32u, flags, 0u, bits, r, g, b, a, 0x1000u, 0u, 0u, 0u, 0u}) appendU32(out, v);
        out.insert(out.end(), img.data.begin(), img.data.end());
        return wrapped(Format::DDS, img.width, img.height, std::move(out));
    };
}

template <>
Stage Decode<Format::DDS>() {
    return [](const Image& img) {
        if (img.data.size() < 128 || readU32(img.data, 0) != fourcc('D', 'D', 'S', ' ') || readU32(img.data, 4) != 124 || readU32(img.data, 76) != 32) {
            throw std::runtime_error("invalid dds header");
        }
        const auto height = static_cast<int>(readU32(img.data, 12));
        const auto width = static_cast<int>(readU32(img.data, 16));
        const auto bits = readU32(img.data, 88);
        const auto r = readU32(img.data, 92);
        const auto g = readU32(img.data, 96);
        const auto b = readU32(img.data, 100);
        const auto a = readU32(img.data, 104);
        Format format;
        if (bits == 16 && r == 0xf800 && g == 0x07e0 && b == 0x001f && a == 0) {
            format = Format::RGB565;
        } else if (bits == 32 && r == 0x000000ff && g == 0x0000ff00 && b == 0x00ff0000 && a == 0xff000000) {
            format = Format::RGBA8888;
        } else {
            throw std::runtime_error("unsupported dds payload format");
        }
        auto data = slice(img.data, 128, payloadSize(format, width, height));
        return wrapped(format, width, height, std::move(data));
    };
}

template <>
Stage Encode<Format::KTX2>() {
    return [](const Image& img) {
        validatePayload(img);
        const auto p = profileFor(img.type, img.width, img.height);
        std::vector<uint8_t> out = {0xab, 0x4b, 0x54, 0x58, 0x20, 0x32, 0x30, 0xbb, 0x0d, 0x0a, 0x1a, 0x0a};
        for (auto v : {p.vkFormat, typeSize(p), static_cast<uint32_t>(img.width),
                       static_cast<uint32_t>(img.height), 0u, 0u, 1u, 1u, 0u, 0u, 0u, 0u, 0u}) appendU32(out, v);
        appendU64(out, 0);
        appendU64(out, 0);
        appendU64(out, 104);
        appendU64(out, img.data.size());
        appendU64(out, img.data.size());
        out.insert(out.end(), img.data.begin(), img.data.end());
        return wrapped(Format::KTX2, img.width, img.height, std::move(out));
    };
}

template <>
Stage Decode<Format::KTX2>() {
    return [](const Image& img) {
        static constexpr std::array<uint8_t, 12> id = {0xab, 0x4b, 0x54, 0x58, 0x20, 0x32, 0x30, 0xbb, 0x0d, 0x0a, 0x1a, 0x0a};
        if (img.data.size() < 104 || !std::equal(id.begin(), id.end(), img.data.begin())) throw std::runtime_error("invalid ktx2 header");
        const auto format = vkFormat(readU32(img.data, 12));
        const auto width = static_cast<int>(readU32(img.data, 20));
        const auto height = static_cast<int>(readU32(img.data, 24));
        if (readU32(img.data, 28) || readU32(img.data, 32) || readU32(img.data, 36) != 1 || readU32(img.data, 40) != 1 || readU32(img.data, 44)) {
            throw std::runtime_error("unsupported ktx2 texture shape");
        }
        const auto off = static_cast<size_t>(readU64(img.data, 80));
        const auto len = static_cast<size_t>(readU64(img.data, 88));
        auto data = slice(img.data, off, len);
        if (data.size() != payloadSize(format, width, height)) throw std::runtime_error("invalid ktx2 payload size");
        return wrapped(format, width, height, std::move(data));
    };
}

}

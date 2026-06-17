#include <haio.hpp>

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace {

std::string lower(std::string_view value) {
    std::string out(value);
    std::ranges::transform(out, out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

std::string_view lastExtension(std::string_view path) {
    const auto slash = path.find_last_of("/\\");
    const auto dot = path.find_last_of('.');
    if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash)) return {};
    return path.substr(dot + 1);
}

}

namespace Haio {

Format formatFromName(std::string_view name) {
    const auto key = lower(name);
    if (key == "png") return Format::PNG;
    if (key == "ppm") return Format::PPM;
    if (key == "webp") return Format::WEBP;
    if (key == "rgba" || key == "rgba8888") return Format::RGBA8888;
    if (key == "rgb" || key == "rgb888") return Format::RGB888;
    if (key == "etc1") return Format::ETC1;
    if (key == "rgb565") return Format::RGB565;
    if (key == "pvr") return Format::PVR;
    if (key == "dds") return Format::DDS;
    if (key == "ktx") return Format::KTX;
    if (key == "ktx2") return Format::KTX2;
    if (key == "raw" || key.empty()) return Format::RAW;
    throw std::runtime_error("unknown format: " + std::string(name));
}

Format formatFromExtension(std::string_view path) {
    return formatFromName(lastExtension(path));
}

std::string_view formatName(Format format) {
    switch (format) {
        case Format::RAW: return "raw";
        case Format::PNG: return "png";
        case Format::PPM: return "ppm";
        case Format::WEBP: return "webp";
        case Format::RGBA8888: return "rgba8888";
        case Format::RGB888: return "rgb888";
        case Format::YUV420: return "yuv420";
        case Format::ETC1: return "etc1";
        case Format::RGB565: return "rgb565";
        case Format::PVR: return "pvr";
        case Format::DDS: return "dds";
        case Format::KTX: return "ktx";
        case Format::KTX2: return "ktx2";
    }
    return "raw";
}

std::string_view extensionFor(Format format) {
    return formatName(format);
}

std::string_view contentTypeFor(Format format) {
    switch (format) {
        case Format::PNG: return "image/png";
        case Format::PPM: return "image/x-portable-pixmap";
        case Format::WEBP: return "image/webp";
        case Format::KTX: return "image/ktx";
        case Format::KTX2: return "image/ktx2";
        case Format::DDS: return "image/vnd-ms.dds";
        case Format::PVR: return "image/x-pvr";
        default: return "application/octet-stream";
    }
}

bool isEncodedImageFormat(Format format) {
    switch (format) {
        case Format::PNG:
        case Format::PPM:
        case Format::WEBP:
        case Format::ETC1:
        case Format::RGB565:
        case Format::PVR:
        case Format::DDS:
        case Format::KTX:
        case Format::KTX2:
            return true;
        default:
            return false;
    }
}

bool isTransformFormat(Format format) {
    return format == Format::RGBA8888 || format == Format::RGB888 || format == Format::RGB565 || format == Format::ETC1;
}

}

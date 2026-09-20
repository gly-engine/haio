#include <haio.hpp>

#include <algorithm>
#include <array>
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
    if (key == "rgba" || key == "rgba8888") return Format::RGBA8888;
    if (key == "rgb" || key == "rgb888") return Format::RGB888;
    if (key == "etc1") return Format::ETC1;
    if (key == "rgb565") return Format::RGB565;
    if (key == "pvr") return Format::PVR;
    if (key == "dds") return Format::DDS;
    if (key == "ktx") return Format::KTX;
    if (key == "ktx2") return Format::KTX2;
    if (key == "zcis") return Format::ZCIS;
    if (key == "raw" || key.empty()) return Format::RAW;
    throw std::runtime_error("unknown format: " + std::string(name));
}

Format formatFromExtension(std::string_view path) {
    return formatFromName(lastExtension(path));
}

Format formatFromMagic(std::span<const uint8_t> data) {
    if (Detect<Format::PNG>(data)) return Format::PNG;
    if (Detect<Format::ZCIS>(data)) return Format::ZCIS;
    if (Detect<Format::KTX2>(data)) return Format::KTX2;
    if (Detect<Format::KTX>(data)) return Format::KTX;
    if (Detect<Format::DDS>(data)) return Format::DDS;
    if (Detect<Format::PVR>(data)) return Format::PVR;
    if (Detect<Format::PPM>(data)) return Format::PPM;
    return Format::RAW;
}

/**
 * formats haio can recognise but not read. naming them turns "unknown input" into
 * something a caller can act on, without giving them an enum entry that has no codec.
 */
std::string_view describeForeignMagic(std::span<const uint8_t> data) {
    const auto starts = [data](std::string_view magic) {
        return data.size() >= magic.size()
            && std::equal(magic.begin(), magic.end(), data.begin(), [](char a, uint8_t b) {
                   return static_cast<uint8_t>(a) == b;
               });
    };

    if (data.size() >= 3 && data[0] == 0xff && data[1] == 0xd8 && data[2] == 0xff) return "jpeg";
    if (starts("GIF87a") || starts("GIF89a")) return "gif";
    if (starts("RIFF") && data.size() >= 12 && starts("RIFF") && std::equal(data.begin() + 8, data.begin() + 12, "WEBP")) return "webp";
    if (starts("BM")) return "bmp";
    if (starts("II*") || starts("MM\0*")) return "tiff";
    if (data.size() >= 4 && data[0] == 0x1f && data[1] == 0x8b) return "gzip";
    return {};
}

Format formatFromContentType(std::string_view contentType) {
    auto value = contentType.substr(0, contentType.find(';'));
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.remove_prefix(1);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.remove_suffix(1);

    const auto key = lower(value);
    if (key == "image/png") return Format::PNG;
    if (key == "image/x-portable-pixmap" || key == "image/x-portable-anymap") return Format::PPM;
    if (key == "image/ktx") return Format::KTX;
    if (key == "image/ktx2") return Format::KTX2;
    if (key == "image/vnd-ms.dds" || key == "image/vnd.ms-dds") return Format::DDS;
    if (key == "image/x-pvr") return Format::PVR;
    if (key == "image/x-zcis") return Format::ZCIS;
    return Format::RAW;
}

/**
 * the bytes win over what the server or the url claim, because those are the two
 * things that lie: gam creatives carry no extension and are often mislabelled.
 */
Format detectFormat(std::span<const uint8_t> data, std::string_view contentType, std::string_view path) {
    if (const auto magic = formatFromMagic(data); magic != Format::RAW) return magic;
    if (const auto declared = formatFromContentType(contentType); declared != Format::RAW) return declared;

    try {
        return formatFromExtension(path);
    } catch (const std::exception&) {
        return Format::RAW;
    }
}

std::string_view formatName(Format format) {
    switch (format) {
        case Format::RAW: return "raw";
        case Format::PNG: return "png";
        case Format::PPM: return "ppm";
        case Format::RGBA8888: return "rgba8888";
        case Format::RGB888: return "rgb888";
        case Format::YUV420: return "yuv420";
        case Format::ETC1: return "etc1";
        case Format::RGB565: return "rgb565";
        case Format::PVR: return "pvr";
        case Format::DDS: return "dds";
        case Format::KTX: return "ktx";
        case Format::KTX2: return "ktx2";
        case Format::ZCIS: return "zcis";
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
        case Format::KTX: return "image/ktx";
        case Format::KTX2: return "image/ktx2";
        case Format::DDS: return "image/vnd-ms.dds";
        case Format::PVR: return "image/x-pvr";
        case Format::ZCIS: return "image/x-zcis";
        default: return "application/octet-stream";
    }
}

bool isEncodedImageFormat(Format format) {
    switch (format) {
        case Format::PNG:
        case Format::PPM:
        case Format::ETC1:
        case Format::RGB565:
        case Format::PVR:
        case Format::DDS:
        case Format::KTX:
        case Format::KTX2:
        case Format::ZCIS:
            return true;
        default:
            return false;
    }
}

bool isTransformFormat(Format format) {
    return format == Format::RGBA8888 || format == Format::RGB888 || format == Format::RGB565 || format == Format::ETC1;
}

}

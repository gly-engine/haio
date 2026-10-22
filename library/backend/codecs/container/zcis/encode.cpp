#include <haio_codec.hpp>
#include <haio/generated/codec.hpp>
#include <haio_convert.hpp>
#include <haio_util.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

namespace {

constexpr std::string_view base62 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
constexpr int base62PairMax = 62 * 62 - 1;

/**
 * zcis splits the canvas into two layers of alternating rows, so that each layer
 * stays a decodable image on its own and both together rebuild the original
 * without losing a pixel.
 */
constexpr char baseCommand = 'A';
constexpr char refineCommand = 'C';

constexpr std::string_view indexMemberName = "0000000000.txt";

std::string base62Pair(int value) {
    return {base62[static_cast<size_t>(value) / 62], base62[static_cast<size_t>(value) % 62]};
}

/** ar members sit on even offsets, padded with a newline. */
std::optional<Haio::Error> appendMember(std::vector<uint8_t>& out, const std::string& name, const std::vector<uint8_t>& body) {
    std::array<char, 61> header{};
    const int written = std::snprintf(
        header.data(),
        header.size(),
        "%-16s%-12u%-6u%-6u%-8o%-10zu`\n",
        name.c_str(),
        0u,
        0u,
        0u,
        0644u,
        body.size()
    );
    if (written != 60) return Haio::Error{Haio::ErrorCode::InvalidInput, "zcis member header overflow: " + name};

    out.insert(out.end(), header.begin(), header.begin() + 60);
    out.insert(out.end(), body.begin(), body.end());
    if (body.size() % 2 != 0) out.push_back('\n');
    return std::nullopt;
}

void appendBytes(std::vector<uint8_t>& out, std::string_view text) {
    out.insert(out.end(), text.begin(), text.end());
}

Haio::Image<Haio::Color::RGBA8888> selectRows(const Haio::Image<Haio::Color::RGBA8888>& image, int parity) {
    const int height = (image.height - parity + 1) / 2;
    const auto stride = static_cast<size_t>(image.width) * 4;

    std::vector<uint8_t> data(stride * static_cast<size_t>(height));
    for (int y = 0; y < height; y++) {
        const auto src = static_cast<size_t>(2 * y + parity) * stride;
        std::copy_n(image.data.data() + src, stride, data.data() + static_cast<size_t>(y) * stride);
    }
    return Haio::Image<Haio::Color::RGBA8888>{image.width, height, std::move(data)};
}

std::optional<Haio::Error> checkCanvas(int width, int height) {
    if (width > base62PairMax || height > base62PairMax) {
        return Haio::Error{Haio::ErrorCode::InvalidInput, "zcis dimensions exceed base62 limit"};
    }
    return std::nullopt;
}

std::optional<Haio::Error> openArchive(std::vector<uint8_t>& out, int width, int height) {
    appendBytes(out, "!<arch>\n");
    const auto index = "0 0 " + std::to_string(width) + " " + std::to_string(height) + "\r\n";
    return appendMember(out, std::string(indexMemberName), {index.begin(), index.end()});
}

std::string canvasTarget(int width, int height) {
    return base62Pair(0) + base62Pair(0) + base62Pair(width) + base62Pair(height);
}

}

namespace Haio::Codecs {

/**
 * @addtogroup encode
 * @{
 */
template <>
Result<Blob> Encode<Format::ZCIS, Color::RGBA8888>(Image<Color::RGBA8888> img) {
    const auto expected = static_cast<size_t>(img.width) * static_cast<size_t>(img.height) * 4;
    if (img.width <= 0 || img.height <= 0 || img.data.size() != expected) {
        HAIO_FAIL(InvalidInput, "invalid rgba8888 image for zcis encode");
    }
    HAIO_CHECK(checkCanvas(img.width, img.height));

    std::vector<uint8_t> out;
    HAIO_CHECK(openArchive(out, img.width, img.height));

    const auto target = canvasTarget(img.width, img.height);

    int order = 1;
    const auto appendLayer = [&](char command, const Image<Color::RGBA8888>& layer) -> std::optional<Error> {
        if (layer.height <= 0) return std::nullopt;

        auto payload = Encode<Format::PPM, Color::RGBA8888>(Image<Color::RGBA8888>{layer});
        if (!payload) return payload.error();

        const auto name = std::string(1, base62[static_cast<size_t>(order++)]) + command + target + ".ppm";
        return appendMember(out, name, payload->data);
    };

    if (img.height == 1) {
        HAIO_CHECK(appendLayer(baseCommand, selectRows(img, 0)));
    } else {
        HAIO_CHECK(appendLayer(baseCommand, selectRows(img, 1)));
        HAIO_CHECK(appendLayer(refineCommand, selectRows(img, 0)));
    }

    return Blob{Format::ZCIS, Color::RGBA8888, "image/x-zcis", {}, std::move(out)};
}

template <>
Result<Blob> Encode<Format::ZCIS, Color::YUV420>(Image<Color::YUV420> img) {
    if (img.width <= 0 || img.height <= 0) HAIO_FAIL(InvalidInput, "invalid yuv420 image for zcis encode");
    HAIO_TRY(expected, sizeOf(Color::YUV420, Size{img.width, img.height}));
    if (img.data.size() != expected) HAIO_FAIL(InvalidInput, "invalid yuv420 image for zcis encode");
    HAIO_CHECK(checkCanvas(img.width, img.height));

    std::vector<uint8_t> out;
    HAIO_CHECK(openArchive(out, img.width, img.height));

    std::vector<uint8_t> payload;
    appendBytes(payload, "YUV4MPEG2 W" + std::to_string(img.width) + " H" + std::to_string(img.height)
                             + " F1:1 Ip A1:1 C420jpeg XCOLORRANGE=FULL\nFRAME\n");
    payload.insert(payload.end(), img.data.begin(), img.data.end());

    const auto name = std::string(1, base62[1]) + baseCommand + canvasTarget(img.width, img.height) + ".y4m";
    HAIO_CHECK(appendMember(out, name, payload));

    return Blob{Format::ZCIS, Color::YUV420, "image/x-zcis", {}, std::move(out)};
}
/** @} */

}

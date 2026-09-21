#include <haio_codec.hpp>
#include <haio_codecs.hpp>
#include <haio_convert.hpp>

#include <optional>
#include <string>

namespace {

using Haio::Bytes;
using Haio::Error;
using Haio::ErrorCode;
using Haio::Result;

/** an image this wide would not fit anyway, and a header can claim it in six bytes */
constexpr size_t maxPixels = size_t{1} << 28;

struct Header {
    int kind = 0;       // the digit in P1 through P6
    int width = 0;
    int height = 0;
    int maxval = 1;
    size_t offset = 0;  // first byte after the header
};

bool isBlank(uint8_t c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/** comments may sit anywhere whitespace may, including between the width and height */
void skipBlanks(Bytes data, size_t& at) {
    while (at < data.size()) {
        if (isBlank(data[at])) {
            at++;
        } else if (data[at] == '#') {
            while (at < data.size() && data[at] != '\n') at++;
        } else {
            break;
        }
    }
}

std::optional<int> readNumber(Bytes data, size_t& at) {
    skipBlanks(data, at);
    if (at >= data.size() || data[at] < '0' || data[at] > '9') return std::nullopt;

    long long value = 0;
    while (at < data.size() && data[at] >= '0' && data[at] <= '9') {
        value = value * 10 + (data[at] - '0');
        if (value > 0x7FFFFFFF) return std::nullopt;
        at++;
    }
    return static_cast<int>(value);
}

Result<Header> readHeader(Bytes data) {
    if (data.size() < 2 || data[0] != 'P' || data[1] < '1' || data[1] > '6') {
        return std::unexpected(Error{ErrorCode::InvalidInput, "not a netpbm header"});
    }

    Header header;
    header.kind = data[1] - '0';
    size_t at = 2;

    const auto width = readNumber(data, at);
    const auto height = readNumber(data, at);
    if (!width || !height) {
        return std::unexpected(Error{ErrorCode::InvalidInput, "netpbm header has no dimensions"});
    }
    header.width = *width;
    header.height = *height;

    // a bitmap has no maxval: every sample is one bit
    if (header.kind != 1 && header.kind != 4) {
        const auto maxval = readNumber(data, at);
        if (!maxval) return std::unexpected(Error{ErrorCode::InvalidInput, "netpbm header has no maxval"});
        header.maxval = *maxval;
    }

    if (header.width <= 0 || header.height <= 0) {
        return std::unexpected(Error{ErrorCode::InvalidInput, "netpbm image has no area"});
    }
    if (header.maxval < 1 || header.maxval > 65535) {
        return std::unexpected(Error{ErrorCode::InvalidInput, "netpbm maxval out of range"});
    }
    if (static_cast<size_t>(header.width) * static_cast<size_t>(header.height) > maxPixels) {
        return std::unexpected(Error{ErrorCode::InvalidInput, "netpbm image is too large"});
    }

    // the binary payload starts one whitespace character after the header, and that
    // character is part of the header rather than a separator that may repeat
    if (at < data.size() && isBlank(data[at])) at++;
    header.offset = at;
    return header;
}

uint8_t scale(int value, int maxval) {
    if (value <= 0) return 0;
    if (value >= maxval) return 255;
    return static_cast<uint8_t>((value * 255 + maxval / 2) / maxval);
}

/**
 * an ascii sample is at least one digit, and all but the last need a separator, so a
 * file shorter than this cannot hold what its header promises. checking before the
 * allocation keeps a six byte header from asking for a gigabyte.
 */
std::optional<Error> checkAsciiRoom(Bytes data, const Header& header, size_t samples) {
    const auto available = data.size() - header.offset;
    if (samples > 0 && available < samples * 2 - 1) {
        return Error{ErrorCode::InvalidInput, "netpbm ascii data is shorter than its header claims"};
    }
    return std::nullopt;
}

std::optional<Error> checkBinaryRoom(Bytes data, const Header& header, size_t bytes) {
    if (data.size() - header.offset < bytes) {
        return Error{ErrorCode::InvalidInput, "netpbm binary data is shorter than its header claims"};
    }
    return std::nullopt;
}

/** P1 writes one bit per token, where 1 means black, the opposite of a grey level */
Result<std::vector<uint8_t>> readAsciiBits(Bytes data, const Header& header, size_t pixels) {
    std::vector<uint8_t> out(pixels);
    size_t at = header.offset;
    for (size_t sample = 0; sample < pixels; sample++) {
        skipBlanks(data, at);
        if (at >= data.size() || (data[at] != '0' && data[at] != '1')) {
            return std::unexpected(Error{ErrorCode::InvalidInput, "netpbm bitmap ran out of bits"});
        }
        out[sample] = data[at] == '1' ? 0 : 255;
        at++;
    }
    return out;
}

Result<std::vector<uint8_t>> readAsciiSamples(Bytes data, const Header& header, size_t samples) {
    std::vector<uint8_t> out(samples);
    size_t at = header.offset;
    for (size_t sample = 0; sample < samples; sample++) {
        const auto value = readNumber(data, at);
        if (!value) return std::unexpected(Error{ErrorCode::InvalidInput, "netpbm ascii data ran out of samples"});
        out[sample] = scale(*value, header.maxval);
    }
    return out;
}

/** P4 packs bits most significant first and pads every row to a whole byte */
Result<std::vector<uint8_t>> readBinaryBits(Bytes data, const Header& header) {
    const auto stride = (static_cast<size_t>(header.width) + 7) / 8;
    if (auto problem = checkBinaryRoom(data, header, stride * static_cast<size_t>(header.height))) {
        return std::unexpected(*problem);
    }

    std::vector<uint8_t> out(static_cast<size_t>(header.width) * static_cast<size_t>(header.height));
    for (size_t y = 0; y < static_cast<size_t>(header.height); y++) {
        const auto row = header.offset + y * stride;
        for (size_t x = 0; x < static_cast<size_t>(header.width); x++) {
            const auto bit = (data[row + x / 8] >> (7 - (x % 8))) & 1;
            out[y * static_cast<size_t>(header.width) + x] = bit ? 0 : 255;
        }
    }
    return out;
}

Result<std::vector<uint8_t>> readBinarySamples(Bytes data, const Header& header, size_t samples) {
    const bool wide = header.maxval > 255;
    const auto width = wide ? size_t{2} : size_t{1};
    if (auto problem = checkBinaryRoom(data, header, samples * width)) return std::unexpected(*problem);

    std::vector<uint8_t> out(samples);
    for (size_t sample = 0; sample < samples; sample++) {
        const auto at = header.offset + sample * width;
        // sixteen bit samples are big endian, whatever the machine is
        const int value = wide ? (data[at] << 8) | data[at + 1] : data[at];
        out[sample] = scale(value, header.maxval);
    }
    return out;
}

}

namespace Haio::Codecs {

/**
 * @addtogroup decode
 * @{
 */
/** the colour pipes: P3 and P6 carry three samples a pixel */
template <>
Result<Image<Color::RGB888>> Decode<Format::PPM, Color::RGB888>(const Blob& blob) {
    HAIO_TRY(header, readHeader(blob.data));
    if (header.kind != 3 && header.kind != 6) {
        HAIO_FAIL(InvalidInput, "P" + std::to_string(header.kind) + " is greyscale, not rgb888");
    }

    const auto samples = static_cast<size_t>(header.width) * static_cast<size_t>(header.height) * 3;
    if (header.kind == 3) HAIO_CHECK(checkAsciiRoom(blob.data, header, samples));

    HAIO_TRY(pixels, header.kind == 3 ? readAsciiSamples(blob.data, header, samples)
                                      : readBinarySamples(blob.data, header, samples));
    return Image<Color::RGB888>{header.width, header.height, std::move(pixels)};
}
/** @} */

/**
 * @addtogroup decode
 * @{
 */
/**
 * the grey pipes: P2 and P5 carry one sample a pixel, P1 and P4 one bit. a bitmap
 * calls 1 black, so the bit is inverted rather than scaled.
 */
template <>
Result<Image<Color::GRAY8>> Decode<Format::PPM, Color::GRAY8>(const Blob& blob) {
    HAIO_TRY(header, readHeader(blob.data));
    if (header.kind == 3 || header.kind == 6) {
        HAIO_FAIL(InvalidInput, "P" + std::to_string(header.kind) + " is rgb888, not greyscale");
    }

    const auto pixels = static_cast<size_t>(header.width) * static_cast<size_t>(header.height);
    if (header.kind == 1 || header.kind == 2) HAIO_CHECK(checkAsciiRoom(blob.data, header, pixels));

    Result<std::vector<uint8_t>> samples =
        header.kind == 1 ? readAsciiBits(blob.data, header, pixels)
      : header.kind == 2 ? readAsciiSamples(blob.data, header, pixels)
      : header.kind == 4 ? readBinaryBits(blob.data, header)
                         : readBinarySamples(blob.data, header, pixels);

    HAIO_TRY(levels, std::move(samples));
    return Image<Color::GRAY8>{header.width, header.height, std::move(levels)};
}
/** @} */

}

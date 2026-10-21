#include <haio_cdn.hpp>

namespace Haio::Cdn {

std::optional<BodyEncoding> bodyEncodingNamed(std::string_view name) {
    if (name.empty() || name == "identity") return BodyEncoding::Identity;
    if (name == "cobs") return BodyEncoding::Cobs;
    if (name == "base64") return BodyEncoding::Base64;
    if (name == "hex") return BodyEncoding::Hex;
    return std::nullopt;
}

std::string_view bodyEncodingName(BodyEncoding encoding) {
    switch (encoding) {
        case BodyEncoding::Identity: return "identity";
        case BodyEncoding::Cobs: return "cobs";
        case BodyEncoding::Base64: return "base64";
        case BodyEncoding::Hex: return "hex";
    }
    return "identity";
}

namespace {

/**
 * each zero is replaced by the distance to the next one, so a zero costs nothing and
 * only a run of 254 bytes without one costs a byte more. the code is written ahead of
 * its block, which is why its place is kept and counted up as the block grows.
 *
 * a full block is only followed by a new one when there is something left to put in
 * it, so the output matches the reference encoder byte for byte.
 */
std::vector<uint8_t> encodeCobs(const std::vector<uint8_t>& body) {
    std::vector<uint8_t> out;
    out.reserve(body.size() + body.size() / 254 + 1);

    size_t code = out.size();
    out.push_back(0x01);

    for (const auto byte : body) {
        if (out[code] == 0xFF) {
            code = out.size();
            out.push_back(0x01);
        }
        if (byte == 0x00) {
            code = out.size();
            out.push_back(0x01);
            continue;
        }
        out.push_back(byte);
        out[code]++;
    }
    return out;
}

std::vector<uint8_t> encodeBase64(const std::vector<uint8_t>& body) {
    static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::vector<uint8_t> out;
    out.reserve((body.size() + 2) / 3 * 4);

    size_t index = 0;
    for (; index + 3 <= body.size(); index += 3) {
        const uint32_t triple = (body[index] << 16) | (body[index + 1] << 8) | body[index + 2];
        out.push_back(alphabet[(triple >> 18) & 0x3F]);
        out.push_back(alphabet[(triple >> 12) & 0x3F]);
        out.push_back(alphabet[(triple >> 6) & 0x3F]);
        out.push_back(alphabet[triple & 0x3F]);
    }

    const auto left = body.size() - index;
    if (left > 0) {
        const uint32_t triple = (body[index] << 16) | (left == 2 ? body[index + 1] << 8 : 0);
        out.push_back(alphabet[(triple >> 18) & 0x3F]);
        out.push_back(alphabet[(triple >> 12) & 0x3F]);
        out.push_back(left == 2 ? alphabet[(triple >> 6) & 0x3F] : '=');
        out.push_back('=');
    }
    return out;
}

std::vector<uint8_t> encodeHex(const std::vector<uint8_t>& body) {
    static constexpr char digits[] = "0123456789abcdef";

    std::vector<uint8_t> out;
    out.reserve(body.size() * 2);
    for (const auto byte : body) {
        out.push_back(digits[byte >> 4]);
        out.push_back(digits[byte & 0x0F]);
    }
    return out;
}

}

std::vector<uint8_t> encodeBody(const std::vector<uint8_t>& body, BodyEncoding encoding) {
    switch (encoding) {
        case BodyEncoding::Identity: return body;
        case BodyEncoding::Cobs: return encodeCobs(body);
        case BodyEncoding::Base64: return encodeBase64(body);
        case BodyEncoding::Hex: return encodeHex(body);
    }
    return body;
}

}

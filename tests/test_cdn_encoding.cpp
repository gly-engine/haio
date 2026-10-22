#include <haio_cdn.hpp>

#include <iostream>
#include <string>

using namespace Haio::Cdn;

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (ok) return;
    std::cerr << "fail: " << what << '\n';
    failures++;
}

std::vector<uint8_t> bytes(std::string text) {
    return {text.begin(), text.end()};
}

std::string text(const std::vector<uint8_t>& body) {
    return {body.begin(), body.end()};
}

std::vector<uint8_t> decodeCobs(const std::vector<uint8_t>& body) {
    std::vector<uint8_t> out;
    size_t index = 0;
    while (index < body.size()) {
        const size_t code = body[index++];
        for (size_t left = code - 1; left > 0 && index < body.size(); left--) out.push_back(body[index++]);
        if (code != 0xFF && index < body.size()) out.push_back(0x00);
    }
    return out;
}

void checkCobs(const std::vector<uint8_t>& plain, const std::vector<uint8_t>& encoded, const std::string& what) {
    check(encodeBody(plain, BodyEncoding::Cobs) == encoded, "cobs encodes " + what);
    check(decodeCobs(encoded) == plain, "cobs decodes " + what);
}

std::vector<uint8_t> range(int first, int last) {
    std::vector<uint8_t> out;
    for (int value = first; value <= last; value++) out.push_back(static_cast<uint8_t>(value));
    return out;
}

std::vector<uint8_t> join(std::vector<uint8_t> left, const std::vector<uint8_t>& right) {
    left.insert(left.end(), right.begin(), right.end());
    return left;
}

}

int main() {
    check(bodyEncodingNamed("cobs") == BodyEncoding::Cobs, "cobs is named");
    check(bodyEncodingNamed("base64") == BodyEncoding::Base64, "base64 is named");
    check(bodyEncodingNamed("hex") == BodyEncoding::Hex, "hex is named");
    check(bodyEncodingNamed("") == BodyEncoding::Identity, "an empty name is the body as it is");
    check(!bodyEncodingNamed("nonull"), "a name haio does not write is refused");
    check(bodyEncodingName(BodyEncoding::Cobs) == "cobs", "cobs spells itself back");

    checkCobs({}, {0x01}, "nothing");
    checkCobs({0x00}, {0x01, 0x01}, "one zero");
    checkCobs({0x00, 0x00}, {0x01, 0x01, 0x01}, "two zeros");
    checkCobs({0x00, 0x11, 0x00}, {0x01, 0x02, 0x11, 0x01}, "a byte between zeros");
    checkCobs({0x11, 0x22, 0x00, 0x33}, {0x03, 0x11, 0x22, 0x02, 0x33}, "a zero in the middle");
    checkCobs({0x11, 0x22, 0x33, 0x44}, {0x05, 0x11, 0x22, 0x33, 0x44}, "no zero at all");
    checkCobs({0x11, 0x00, 0x00, 0x00}, {0x02, 0x11, 0x01, 0x01, 0x01}, "trailing zeros");
    checkCobs(range(1, 254), join({0xFF}, range(1, 254)), "a block that is exactly full");
    checkCobs(range(0, 254), join({0x01, 0xFF}, range(1, 254)), "a zero and then a full block");
    checkCobs(range(1, 255), join(join({0xFF}, range(1, 254)), {0x02, 0xFF}), "one byte past a full block");

    std::vector<uint8_t> every;
    for (int round = 0; round < 3; round++) every = join(every, range(0, 255));
    const auto stuffed = encodeBody(every, BodyEncoding::Cobs);
    check(std::ranges::find(stuffed, 0x00) == stuffed.end(), "cobs leaves no zero byte");
    check(decodeCobs(stuffed) == every, "every byte survives cobs");

    const std::vector<uint8_t> black(192015, 0x00);
    check(encodeBody(black, BodyEncoding::Cobs).size() == black.size() + 1, "a black picture costs one byte");

    check(text(encodeBody(bytes(""), BodyEncoding::Base64)).empty(), "base64 of nothing is nothing");
    check(text(encodeBody(bytes("f"), BodyEncoding::Base64)) == "Zg==", "base64 pads one byte");
    check(text(encodeBody(bytes("fo"), BodyEncoding::Base64)) == "Zm8=", "base64 pads two bytes");
    check(text(encodeBody(bytes("foo"), BodyEncoding::Base64)) == "Zm9v", "base64 of three bytes has no padding");
    check(text(encodeBody(bytes("foobar"), BodyEncoding::Base64)) == "Zm9vYmFy", "base64 follows rfc 4648");
    check(text(encodeBody({0x00, 0xFF, 0xFE}, BodyEncoding::Base64)) == "AP/+", "base64 uses + and /");

    check(text(encodeBody({0x00, 0x0F, 0xA0, 0xFF}, BodyEncoding::Hex)) == "000fa0ff", "hex is two lowercase digits a byte");
    check(encodeBody(every, BodyEncoding::Identity) == every, "identity hands the body back");

    if (failures == 0) std::cout << "test_cdn_encoding: ok\n";
    return failures == 0 ? 0 : 1;
}

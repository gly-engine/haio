#pragma once

#include "haio_common.hpp"
#include "haio_formats.hpp"

#include <optional>
#include <unordered_map>

namespace Haio {

struct Image;

struct Blob {
    Format format = Format::RAW;
    std::string contentType = "application/octet-stream";
    std::string path;
    std::vector<uint8_t> data;
};

struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct Size {
    int width = 0;
    int height = 0;
};

enum class TokenKind {
    Source,
    DecodeAuto,
    Decode,
    Crop,
    Resize,
    Radius,
    Encode
};

struct Token {
    TokenKind kind = TokenKind::DecodeAuto;
    std::string bucket;
    std::string path;
    Format format = Format::RAW;
    Rect rect;
    Size size;
    int radius = 0;
};

class Pipeline {
public:
    Pipeline& operator|=(Token token);
    const std::vector<Token>& tokens() const;

private:
    std::vector<Token> tokens_;
};

namespace Tokens {
Token Source(std::string bucket, std::string path);
Token DecodeAuto();
Token Decode(Format format);
Token Crop(Rect rect);
Token Resize(Size size);
Token Radius(int radius);
Token Encode(Format format);
}

Format formatFromName(std::string_view name);
Format formatFromExtension(std::string_view path);
std::string_view formatName(Format format);
std::string_view extensionFor(Format format);
std::string_view contentTypeFor(Format format);
bool isEncodedImageFormat(Format format);
bool isTransformFormat(Format format);

Image decodeBlob(const Blob& blob, Format requested = Format::RAW);
Blob encodeImage(const Image& image, Format format, std::string path = {});
Blob runPipeline(Blob input, const Pipeline& pipeline);

Image cropImage(const Image& image, Rect rect);
Image resizeImage(const Image& image, Size size);
Image roundImageCorners(const Image& image, int radius);

std::vector<Token> parseQueryTokens(std::string_view query);
std::unordered_map<std::string, std::string> parseQueryMap(std::string_view query);

}

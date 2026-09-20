#pragma once

#include "haio_codec.hpp"
#include "haio_common.hpp"
#include "haio_formats.hpp"
#include "haio_object.hpp"
#include "haio_transform.hpp"

#include <span>
#include <unordered_map>

namespace Haio {

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

/** a conversion described at runtime, which is what a url query builds */
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

Format formatFromExtension(std::string_view path);
Format formatFromContentType(std::string_view contentType);
std::string_view extensionFor(Format format);
std::string_view contentTypeFor(Format format);

Result<Blob> runPipeline(Blob input, const Pipeline& pipeline);

std::vector<Token> parseQueryTokens(std::string_view query);
std::unordered_map<std::string, std::string> parseQueryMap(std::string_view query);

}

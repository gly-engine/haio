#pragma once

#include "haio_codec.hpp"
#include "haio_common.hpp"
#include "haio_formats.hpp"
#include "haio_object.hpp"
#include "haio_palette.hpp"
#include "haio_transform.hpp"

#include <chrono>
#include <optional>
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
    Palette,
    Encode
};

struct Token {
    TokenKind kind = TokenKind::DecodeAuto;
    std::string bucket;
    std::string path;
    Format format = Format::RAW;
    Rect rect;
    Size size;

    /** Resize only: a share of the incoming picture, when the size was written as one */
    int percent = 0;
    int radius = 0;

    /** Palette only: which colours, how to fit into them, and how many to keep */
    std::string palette;
    Dither dither = Dither::Nearest;
    size_t limit = 0;
    Limit limitHow = Limit::Spread;
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
Token ResizeByPercent(int percent);
Token Radius(int radius);
Token Palette(std::string palette, Dither dither, size_t limit, Limit limitHow);
Token Encode(Format format);
}

Format formatFromExtension(std::string_view path);
Format formatFromContentType(std::string_view contentType);
std::string_view extensionFor(Format format);
std::string_view contentTypeFor(Format format);

/**
 * a deadline of none lets it run as long as it takes.
 *
 * it is checked between stages rather than inside them, so the bound is really "one
 * more stage after the clock runs out". that is enough for what it defends against: a
 * query naming a thousand operations is stopped after the first one or two, and a
 * single stage is already bounded by max_size.
 */
Result<Blob> runPipeline(Blob input, const Pipeline& pipeline,
                         std::optional<std::chrono::steady_clock::time_point> deadline = std::nullopt);

std::vector<Token> parseQueryTokens(std::string_view query);
std::unordered_map<std::string, std::string> parseQueryMap(std::string_view query);

}

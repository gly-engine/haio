#pragma once

#include "haio_codec.hpp"
#include "haio_common.hpp"
#include "haio_formats.hpp"
#include "haio_object.hpp"
#include "haio_palette.hpp"
#include "haio/transforms/composite.hpp"
#include "haio/transforms/resize.hpp"

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
    Composite,
    Fx,
    Open,    /**< a parenthesis: what comes after it, until Close, sees only its own pictures */
    Close,
    Encode
};

struct Token {
    TokenKind kind = TokenKind::DecodeAuto;
    std::string bucket;
    std::string path;
    Format format = Format::RAW;

    /**
     * Encode only: which colour to store inside the container, when somebody named
     * one. nothing named is not the same as rgba8888: it means the container picks,
     * which for most of them is the only colour they write anyway.
     */
    std::optional<Color> color;

    Rect rect;
    Size size;

    /** Resize only: a share of the incoming picture, when the size was written as one */
    int percent = 0;
    ResizeFilter filter = ResizeFilter::Point;   /**< and how the pixels are picked */
    int radius = 0;

    /** Palette only: which colours, how to fit into them, and how many to keep */
    std::string palette;
    Dither dither = Dither::Nearest;
    size_t limit = 0;
    Limit limitHow = Limit::Spread;

    /** Composite only: where the second picture goes on the first; the offset is rect.x and rect.y */
    Gravity gravity = Gravity::NorthWest;

    /** Fx only: the expression, read and not run */
    std::string expression;

    /** Decode and Encode: what the codec reads, as -quality or -define wrote it */
    Settings settings;
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
Token Decode(Format format, Settings settings = {});
Token Crop(Rect rect);
Token Resize(Size size, ResizeFilter filter = ResizeFilter::Point);
Token ResizeByPercent(int percent, ResizeFilter filter = ResizeFilter::Point);
Token Radius(int radius);
Token Palette(std::string palette, Dither dither, size_t limit, Limit limitHow);
Token Composite(Gravity gravity, int x, int y);
Token Fx(std::string expression);
Token Open();
Token Close();
Token Encode(Format format, std::optional<Color> color = std::nullopt, Settings settings = {});
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

/**
 * the same, with a picture for every source: each Decode takes the next input and
 * puts it on a stack, transforms change the pictures of the parenthesis they are in,
 * and a merge such as Composite turns two into one. the encode wants exactly one
 * left, the way a line with one output does.
 */
Result<Blob> runPipeline(std::vector<Blob> inputs, const Pipeline& pipeline,
                         std::optional<std::chrono::steady_clock::time_point> deadline = std::nullopt);

std::vector<Token> parseQueryTokens(std::string_view query);
std::unordered_map<std::string, std::string> parseQueryMap(std::string_view query);

}

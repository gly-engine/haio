#pragma once

#include "haio/domain.hpp"
#include "haio/implementation/geometry.hpp"
#include "haio/implementation/image.hpp"
#include "haio/implementation/wip/dither.hpp"
#include "haio/implementation/codecs.hpp"
#include "haio/implementation/transforms.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

/**
 * what the pipeline runs: one Token per stage, in the order it was written. every
 * front end ends up here, the command line through the parser and the cdn through
 * its query, so a token is the one thing all of them agree on.
 */
namespace Haio::Implementation::Language {

enum class TokenKind {
    Source,
    Generate,   /**< a picture painted by a brush, which reads no input */
    DecodeAuto,
    Decode,
    Crop,
    Resize,
    Radius,
    Negate,
    Palette,
    PixFmt,     /**< the picture moved into the colour in color */
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
     * Encode: which colour to store inside the container, when somebody named
     * one; PixFmt: the colour the picture is moved into. nothing named is not the same as rgba8888: it means the container picks,
     * which for most of them is the only colour they write anyway.
     */
    std::optional<Color> color;

    Rect rect;
    Size size;

    /** Resize only: a share of the incoming picture, when the size was written as one */
    int percent = 0;
    Transforms::Transform<>::ResizeFilter filter = Transforms::Transform<>::ResizeFilter::Point;   /**< and how the pixels are picked */
    int radius = 0;

    /** Palette only: which colours, how to fit into them, and how many to keep */
    std::string palette;
    WIP::Dither dither = WIP::Dither::Nearest;
    size_t limit = 0;
    WIP::Limit limitHow = WIP::Limit::Spread;

    /** Composite only: where the second picture goes on the first; the offset is rect.x and rect.y */
    Transforms::Transform<>::Gravity gravity = Transforms::Transform<>::Gravity::NorthWest;
    Transforms::Compose compose = Transforms::Compose::Over;   /**< and how the two are mixed */

    /** Generate only: which brush; what it paints is in expression and its options in settings */
    Codecs::Brush brush = Codecs::Brush::Xc;

    /** Fx only: the expression, read and not run */
    std::string expression;

    /** Decode, Encode and Generate: what the codec reads, as -quality or -define wrote it */
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

}

/** a token for each stage, made the way the stage asks */
namespace Haio::Implementation::LanguageToken {

Language::Token Source(std::string bucket, std::string path);
Language::Token Generate(Codecs::Brush brush, std::string words, Settings settings = {});
Language::Token DecodeAuto();
Language::Token Decode(Format format, Settings settings = {});
Language::Token Crop(Rect rect);
Language::Token Resize(Size size, Transforms::Transform<>::ResizeFilter filter = Transforms::Transform<>::ResizeFilter::Point);
Language::Token ResizeByPercent(int percent, Transforms::Transform<>::ResizeFilter filter = Transforms::Transform<>::ResizeFilter::Point);
Language::Token Radius(int radius);
Language::Token Negate();
Language::Token Palette(std::string palette, WIP::Dither dither, size_t limit, WIP::Limit limitHow);
Language::Token PixFmt(Color color);
Language::Token Composite(Transforms::Compose compose, Transforms::Transform<>::Gravity gravity, int x, int y);
Language::Token Fx(std::string expression);
Language::Token Open();
Language::Token Close();
Language::Token Encode(Format format, std::optional<Color> color = std::nullopt, Settings settings = {});
}

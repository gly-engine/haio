#include <haio.hpp>

#include <optional>

namespace Haio {

/**
 * a conversion described at runtime, which is what a url query builds. it decodes
 * once, applies whatever transforms were asked for, and encodes at the end.
 *
 * @todo everything travels as rgba8888 here, so a pipeline with no transforms still
 * decompresses a gpu payload and recompresses it. the typed pipe in haio_pipe.hpp
 * keeps the colour; only this runtime path flattens it.
 */
Result<Blob> runPipeline(Blob input, const Pipeline& pipeline,
                         std::optional<std::chrono::steady_clock::time_point> deadline) {
    bool hasImage = false;
    Image<Color::RGBA8888> image;
    Format outputFormat = Format::RAW;
    std::optional<Error> failure;

    auto ensureImage = [&] {
        if (hasImage || failure) return;
        auto decoded = Decode(input);
        if (!decoded) {
            failure = decoded.error();
            return;
        }
        image = *std::move(decoded);
        hasImage = true;
    };

    for (const auto& token : pipeline.tokens()) {
        if (deadline && std::chrono::steady_clock::now() >= *deadline) {
            return std::unexpected(Error{ErrorCode::Timeout, "the conversion took too long and was stopped"});
        }

        switch (token.kind) {
            case TokenKind::Source:
                break;
            case TokenKind::DecodeAuto:
            case TokenKind::Decode:
                ensureImage();
                break;
            case TokenKind::Crop:
                ensureImage();
                if (!failure) image = cropImage(image, token.rect);
                break;
            case TokenKind::Resize: {
                ensureImage();
                if (failure) break;

                // a share is worked out here and not at parsing, because this is the
                // first moment anybody knows what it is a share of
                auto wanted = token.size;
                if (token.percent != 0) {
                    wanted = Size{std::max(1, image.width * token.percent / 100),
                                  std::max(1, image.height * token.percent / 100)};
                }
                image = resizeImage(image, wanted);
                break;
            }
            case TokenKind::Radius:
                ensureImage();
                if (!failure) image = roundImageCorners(image, token.radius);
                break;

            /**
             * quantise, then expand back to full colour.
             *
             * the picture afterwards holds only the palette's colours, which is the
             * point, but it stays rgba8888 so that everything downstream keeps
             * working. a format that stores indices rather than colours would take
             * the palette image itself, and that is a separate road.
             */
            case TokenKind::Palette: {
                ensureImage();
                if (failure) break;

                auto colours = paletteNamed(token.palette, 0);
                if (!colours) {
                    failure = colours.error();
                    break;
                }
                // cutting the palette down comes first, so the dither only ever sees
                // the colours that survived and spreads error among those
                if (token.limit != 0) {
                    auto fewer = limitPalette(image, *std::move(colours), token.limit, token.limitHow);
                    if (!fewer) {
                        failure = fewer.error();
                        break;
                    }
                    colours = *std::move(fewer);
                }

                auto indexed = toPalette(image, *std::move(colours), token.dither);
                if (!indexed) {
                    failure = indexed.error();
                    break;
                }
                auto expanded = Codecs::Convert<Color::PALETTE, Color::RGBA8888>(*std::move(indexed));
                if (!expanded) {
                    failure = expanded.error();
                    break;
                }
                image = *std::move(expanded);
                break;
            }
            case TokenKind::Encode:
                outputFormat = token.format;
                break;
        }
        if (failure) return std::unexpected(*failure);
    }

    if (outputFormat == Format::RAW) {
        if (!hasImage) return input;
        return Blob{Format::RAW, Color::RGBA8888, std::string(contentTypeFor(Format::RAW)),
                    input.path, std::move(image.data)};
    }

    ensureImage();
    if (failure) return std::unexpected(*failure);

    auto encoded = Encode(std::move(image), outputFormat);
    if (encoded) encoded->path = input.path;
    return encoded;
}

}

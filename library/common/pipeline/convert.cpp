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
    std::optional<Color> outputColor;
    std::optional<Error> failure;

    /**
     * the picture as indices, kept beside the full colour one from the moment a
     * palette is applied.
     *
     * quantising and then expanding back loses nothing about the picture and
     * everything about how it is stored, and a container that keeps a palette wants
     * the second thing. the two travel together rather than the pipeline switching
     * over, because every transform downstream is written against full colour and
     * only some of them mean anything to a page of indices.
     */
    std::optional<Image<Color::PALETTE>> indexed;

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
                if (failure) break;
                image = cropImage(image, token.rect);
                // the same rectangle of the same picture: indices are addressable, so
                // there is nothing here that full colour has and they do not
                if (indexed) indexed = cropImage(*indexed, token.rect);
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
                if (indexed) indexed = resizeImage(*indexed, wanted);
                break;
            }
            case TokenKind::Radius:
                ensureImage();
                if (failure) break;
                image = roundImageCorners(image, token.radius);
                // rounding a corner away is done by clearing an alpha, and a palette
                // has nowhere to keep one: from here the picture is full colour only
                indexed.reset();
                break;

            /**
             * quantise, keep the indices, and expand back to full colour.
             *
             * the picture afterwards holds only the palette's colours, which is the
             * point, and it stays rgba8888 so that everything downstream keeps
             * working. the indices are kept as well rather than thrown away, because
             * a container that stores a palette is the one place where they are not
             * simply a longer way of saying the same picture.
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

                auto fitted = toPalette(image, *std::move(colours), token.dither);
                if (!fitted) {
                    failure = fitted.error();
                    break;
                }
                auto expanded = Codecs::Convert<Color::PALETTE, Color::RGBA8888>(*fitted);
                if (!expanded) {
                    failure = expanded.error();
                    break;
                }
                image = *std::move(expanded);
                indexed = *std::move(fitted);
                break;
            }
            case TokenKind::Encode:
                outputFormat = token.format;
                outputColor = token.color;
                break;
        }
        if (failure) return std::unexpected(*failure);
    }

    /**
     * no container asked for, so the answer is the pixels themselves.
     *
     * a colour without a container is still worth doing: "-pix_fmt rgb565 raw:out.bin"
     * is a texture in the layout it will be uploaded in, and it is the same thing
     * ffmpeg writes when it is given a pixel format and no muxer.
     */
    if (outputFormat == Format::RAW) {
        if (!hasImage && !outputColor) return input;

        ensureImage();
        if (failure) return std::unexpected(*failure);

        auto pixels = outputColor ? Convert(image, *outputColor)
                                  : Result<std::vector<uint8_t>>{std::move(image.data)};
        if (!pixels) return std::unexpected(pixels.error());

        return Blob{Format::RAW, outputColor.value_or(Color::RGBA8888),
                    std::string(contentTypeFor(Format::RAW)), input.path, *std::move(pixels)};
    }

    ensureImage();
    if (failure) return std::unexpected(*failure);

    /**
     * a picture that was fitted to a palette goes out as indices wherever the
     * container keeps them, which is the whole reason the indices were kept. asking
     * for any other colour is honoured: somebody who says "-palete cga -pix_fmt
     * bgr888" wants sixteen colours stored the long way, and that is a real thing to
     * want when the reader on the other end has no palette support.
     */
    const bool asIndices = indexed && encodable(outputFormat, Color::PALETTE)
                        && (!outputColor || *outputColor == Color::PALETTE);
    if (outputColor == Color::PALETTE && !indexed) {
        return std::unexpected(Error{ErrorCode::InvalidInput,
                                     "a palette is colours a picture was fitted to, and this one was not; "
                                     "name some with -palete"});
    }

    auto encoded = asIndices ? Encode(*std::move(indexed), outputFormat)
                             : Encode(std::move(image), outputFormat, outputColor);
    if (encoded) encoded->path = input.path;
    return encoded;
}

}

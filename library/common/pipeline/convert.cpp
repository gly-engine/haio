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
Result<Blob> runPipeline(Blob input, const Pipeline& pipeline) {
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
            case TokenKind::Resize:
                ensureImage();
                if (!failure) image = resizeImage(image, token.size);
                break;
            case TokenKind::Radius:
                ensureImage();
                if (!failure) image = roundImageCorners(image, token.radius);
                break;
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

#include <haio.hpp>

#include <optional>

namespace Haio {
namespace {

/**
 * one picture on the stack, in whichever form it is in.
 *
 * it stays in the colour it was stored in until a transform needs it in rgba8888,
 * which is the only colour the transforms are written against. a pipeline with none
 * of them goes from the decoded colour to the encoded one directly: a png of rgb
 * becomes a jpeg without an alpha plane in between, and a jpeg re-encoded as a jpeg
 * never leaves yuv.
 */
struct Picture {
    std::optional<AnyImage> native;
    Image<Color::RGBA8888> image;

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

    /** what every transform starts with: the picture in rgba8888, from here on for good */
    std::optional<Error> rgba() {
        if (!native) return std::nullopt;
        auto converted = Convert(*std::move(native), Color::RGBA8888);
        native.reset();
        if (!converted) return converted.error();
        image = Image<Color::RGBA8888>{converted->width, converted->height, std::move(converted->data)};
        return std::nullopt;
    }

    /** whichever of the two is holding the picture, handed over to the encode */
    AnyImage current() {
        return native ? *std::move(native)
                      : AnyImage{Color::RGBA8888, image.width, image.height, std::move(image.data)};
    }
};

/** a transform's answer, kept where it goes or handed back as the failure */
template <typename I>
std::optional<Error> keep(I& into, Result<I> result) {
    if (!result) return result.error();
    into = *std::move(result);
    return std::nullopt;
}

std::optional<Error> crop(Picture& picture, const Token& token) {
    if (auto failed = picture.rgba()) return failed;
    if (auto failed = keep(picture.image, Transforms::Crop<Color::RGBA8888>(std::move(picture.image), token.rect))) {
        return failed;
    }
    // the same rectangle of the indices too, wherever the build has one for them
    if constexpr (Transforms::Croppable<Color::PALETTE>) {
        if (picture.indexed) return keep(*picture.indexed, Transforms::Crop<Color::PALETTE>(*std::move(picture.indexed), token.rect));
    } else {
        picture.indexed.reset();
    }
    return std::nullopt;
}

std::optional<Error> resize(Picture& picture, const Token& token) {
    if (auto failed = picture.rgba()) return failed;

    // a share is worked out here and not at parsing, because this is the first
    // moment anybody knows what it is a share of
    auto wanted = token.size;
    if (token.percent != 0) {
        wanted = Size{std::max(1, picture.image.width * token.percent / 100),
                      std::max(1, picture.image.height * token.percent / 100)};
    }
    if (auto failed = keep(picture.image, Transforms::Resize<Color::RGBA8888>(std::move(picture.image), wanted, token.filter))) {
        return failed;
    }
    // indices cannot be mixed, so only a point resize keeps them in step
    if constexpr (Transforms::Resizable<Color::PALETTE>) {
        if (picture.indexed && token.filter == ResizeFilter::Point) {
            return keep(*picture.indexed, Transforms::Resize<Color::PALETTE>(*std::move(picture.indexed), wanted, token.filter));
        }
    }
    picture.indexed.reset();
    return std::nullopt;
}

std::optional<Error> radius(Picture& picture, const Token& token) {
    if (auto failed = picture.rgba()) return failed;
    // rounding a corner away is done by clearing an alpha, and a palette has nowhere
    // to keep one: from here the picture is full colour only
    picture.indexed.reset();
    return keep(picture.image, Transforms::Radius<Color::RGBA8888>(std::move(picture.image), token.radius));
}

/**
 * quantise, keep the indices, and expand back to full colour.
 *
 * the picture afterwards holds only the palette's colours, which is the point, and
 * it stays rgba8888 so that everything downstream keeps working. the indices are kept
 * as well rather than thrown away, because a container that stores a palette is the
 * one place where they are not simply a longer way of saying the same picture.
 */
std::optional<Error> palette(Picture& picture, const Token& token) {
    if (auto failed = picture.rgba()) return failed;

    auto colours = paletteNamed(token.palette, 0);
    if (!colours) return colours.error();

    // cutting the palette down comes first, so the dither only ever sees the colours
    // that survived and spreads error among those
    if (token.limit != 0) {
        auto fewer = limitPalette(picture.image, *std::move(colours), token.limit, token.limitHow);
        if (!fewer) return fewer.error();
        colours = *std::move(fewer);
    }

    auto fitted = toPalette(picture.image, *std::move(colours), token.dither);
    if (!fitted) return fitted.error();
    auto expanded = Codecs::Convert<Color::PALETTE, Color::RGBA8888>(*fitted);
    if (!expanded) return expanded.error();

    picture.image = *std::move(expanded);
    picture.indexed = *std::move(fitted);
    return std::nullopt;
}

/**
 * the second picture laid over the first, the way imagemagick's -composite takes the
 * first two of its list. the result is always full colour with an alpha, so the
 * indices of a palette go: a layer drawn over them is colours the palette never had.
 */
std::optional<Error> composite(Picture& base, Picture layer, const Token& token) {
    if (auto failed = base.rgba()) return failed;
    if (auto failed = layer.rgba()) return failed;
    base.indexed.reset();
    return keep(base.image, Transforms::Composite<Color::RGBA8888>(std::move(base.image), layer.image, token.gravity,
                                                                    token.rect.x, token.rect.y));
}

}

Result<Blob> runPipeline(Blob input, const Pipeline& pipeline,
                         std::optional<std::chrono::steady_clock::time_point> deadline) {
    std::vector<Blob> inputs;
    inputs.push_back(std::move(input));
    return runPipeline(std::move(inputs), pipeline, deadline);
}

/**
 * a conversion described at runtime, which is what a url query and a command line
 * both build. every Decode puts a picture on the stack, a transform changes the one
 * on top, merges turn the two of a parenthesis into one, and the encode at the end
 * takes the one that is left.
 *
 * a pipeline with no Decode at all is the cdn's: one input, decoded the moment
 * something needs its pixels, and handed back untouched when nothing does.
 */
Result<Blob> runPipeline(std::vector<Blob> inputs, const Pipeline& pipeline,
                         std::optional<std::chrono::steady_clock::time_point> deadline) {
    std::vector<Picture> stack;
    /** where each open parenthesis begins on the stack; the first is the line itself */
    std::vector<size_t> scopes{0};
    size_t nextInput = 0;
    const std::string path = inputs.empty() ? std::string{} : inputs.front().path;

    Format outputFormat = Format::RAW;
    std::optional<Color> outputColor;
    Settings encodeSettings;

    /** the next input, decoded and put on top */
    auto decode = [&](const Settings& settings) -> std::optional<Error> {
        if (nextInput >= inputs.size()) return Error{ErrorCode::InvalidInput, "more pictures asked for than were given"};
        auto& input = inputs[nextInput++];
        auto decoded = DecodeNative(input, settings);
        if (!decoded) return decoded.error();
        stack.push_back(Picture{*std::move(decoded), {}, std::nullopt});

        // the file has become pixels and nothing reads it again, so it goes now
        // rather than sitting beside every buffer the rest of the pipeline allocates
        input.data = {};
        return std::nullopt;
    };

    /** the cdn's single input, which nobody has decoded yet */
    auto decodeLazily = [&]() -> std::optional<Error> {
        if (stack.empty() && nextInput == 0 && !inputs.empty()) return decode({});
        return std::nullopt;
    };

    /** the pictures of the innermost parenthesis, which is what a merge looks at */
    auto inScope = [&]() -> Result<std::span<Picture>> {
        if (auto failed = decodeLazily()) return std::unexpected(*failed);
        const auto from = scopes.back();
        if (stack.size() <= from) return std::unexpected(Error{ErrorCode::InvalidInput, "there is no picture here to change"});
        return std::span<Picture>{stack}.subspan(from);
    };

    /**
     * a transform changes the picture on top: the one made last, inside the innermost
     * parenthesis. in "xc:blue xc:red -resize 1x1" that is the red one alone, where
     * imagemagick would resize both.
     */
    auto onTop = [&](auto&& change, const Token& token) -> std::optional<Error> {
        auto pictures = inScope();
        if (!pictures) return pictures.error();
        return change(pictures->back(), token);
    };

    for (const auto& token : pipeline.tokens()) {
        if (deadline && std::chrono::steady_clock::now() >= *deadline) {
            return std::unexpected(Error{ErrorCode::Timeout, "the conversion took too long and was stopped"});
        }

        std::optional<Error> failure;
        switch (token.kind) {
            case TokenKind::Source:
                break;
            case TokenKind::DecodeAuto:
            case TokenKind::Decode:
                failure = decode(token.settings);
                break;
            case TokenKind::Crop: failure = onTop(crop, token); break;
            case TokenKind::Resize: failure = onTop(resize, token); break;
            case TokenKind::Radius: failure = onTop(radius, token); break;
            case TokenKind::Palette: failure = onTop(palette, token); break;

            case TokenKind::Composite: {
                auto pictures = inScope();
                if (!pictures) {
                    failure = pictures.error();
                    break;
                }
                // @todo imagemagick takes a third picture as the mask
                if (pictures->size() != 2) {
                    failure = Error{ErrorCode::InvalidInput, "-composite joins two pictures, and there are "
                                                                 + std::to_string(pictures->size())};
                    break;
                }
                auto layer = std::move(stack.back());
                stack.pop_back();
                failure = composite(stack.back(), std::move(layer), token);
                break;
            }

            case TokenKind::Open:
                scopes.push_back(stack.size());
                break;
            case TokenKind::Close:
                if (scopes.size() == 1) {
                    failure = Error{ErrorCode::InvalidInput, "a parenthesis closed that was never opened"};
                    break;
                }
                scopes.pop_back();
                break;

            // read by the command line so its lines parse, and not something that runs
            case TokenKind::Fx:
                failure = Error{ErrorCode::UnsupportedFormat, "-fx is not supported by the pipeline yet"};
                break;

            case TokenKind::Encode:
                outputFormat = token.format;
                outputColor = token.color;
                encodeSettings = token.settings;
                break;
        }
        if (failure) return std::unexpected(*failure);
    }

    /**
     * no container asked for and nothing decoded: the cdn's untouched file, which is
     * what a pipeline with nothing to do hands back.
     */
    if (outputFormat == Format::RAW && !outputColor && stack.empty() && nextInput == 0 && inputs.size() == 1) {
        return std::move(inputs.front());
    }

    if (auto failed = decodeLazily()) return std::unexpected(*failed);
    if (stack.size() != 1) {
        return std::unexpected(Error{ErrorCode::InvalidInput, std::to_string(stack.size())
                                                                  + " pictures and one output; join them with -composite"});
    }
    auto& picture = stack.front();

    /**
     * no container asked for, so the answer is the pixels themselves.
     *
     * a colour without a container is still worth doing: "-pix_fmt rgb565 raw:out.bin"
     * is a texture in the layout it will be uploaded in, and it is the same thing
     * ffmpeg writes when it is given a pixel format and no muxer.
     */
    if (outputFormat == Format::RAW) {
        const auto to = outputColor.value_or(Color::RGBA8888);
        auto pixels = Convert(picture.current(), to);
        if (!pixels) return std::unexpected(pixels.error());

        return Blob{Format::RAW, to, std::string(contentTypeFor(Format::RAW)), path, std::move(pixels->data),
                    Size{pixels->width, pixels->height}};
    }

    /**
     * a picture that was fitted to a palette goes out as indices wherever the
     * container keeps them, which is the whole reason the indices were kept. asking
     * for any other colour is honoured: somebody who says "-palete cga -pix_fmt
     * bgr888" wants sixteen colours stored the long way, and that is a real thing to
     * want when the reader on the other end has no palette support.
     */
    const bool asIndices = picture.indexed && encodable(outputFormat, Color::PALETTE)
                        && (!outputColor || *outputColor == Color::PALETTE);
    if (outputColor == Color::PALETTE && !picture.indexed) {
        return std::unexpected(Error{ErrorCode::InvalidInput,
                                     "a palette is colours a picture was fitted to, and this one was not; "
                                     "name some with -palete"});
    }

    auto encoded = asIndices ? Encode(*std::move(picture.indexed), outputFormat, encodeSettings)
                             : Encode(picture.current(), outputFormat, outputColor, encodeSettings);
    if (encoded) encoded->path = path;
    return encoded;
}

}

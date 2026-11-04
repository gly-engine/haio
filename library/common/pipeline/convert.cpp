#include <haio.hpp>
#include <haio_atlas.hpp>

#include <meta>
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

    Metadata meta;

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

Size sizeOf(const Picture& picture) {
    return picture.native ? Size{picture.native->width, picture.native->height}
                          : Size{picture.image.width, picture.image.height};
}

/**
 * the cuts moved by an offset and trimmed to a canvas this big. one that ends up
 * wholly outside it is dropped, since there is nothing left there to cut out.
 */
std::vector<Cut> movedCuts(std::vector<Cut> cuts, int dx, int dy, Size canvas) {
    std::vector<Cut> kept;
    kept.reserve(cuts.size());
    for (auto& cut : cuts) {
        const auto left = std::max(cut.rect.x + dx, 0);
        const auto top = std::max(cut.rect.y + dy, 0);
        const auto right = std::min(cut.rect.x + dx + cut.rect.width, canvas.width);
        const auto bottom = std::min(cut.rect.y + dy + cut.rect.height, canvas.height);
        if (right <= left || bottom <= top) continue;
        kept.push_back(Cut{Rect{left, top, right - left, bottom - top}, std::move(cut.name)});
    }
    return kept;
}

/**
 * the cuts stretched the way the picture under them was. both edges are scaled, not
 * the corner and the size, so two cuts that touched still touch afterwards, and one
 * shrunk below a pixel keeps the pixel it starts on rather than vanishing.
 */
std::vector<Cut> scaledCuts(std::vector<Cut> cuts, Size from, Size to) {
    const auto scale = [](int at, int wanted, int was) {
        return static_cast<int>(static_cast<int64_t>(at) * wanted / was);
    };
    for (auto& cut : cuts) {
        const auto left = scale(cut.rect.x, to.width, from.width);
        const auto top = scale(cut.rect.y, to.height, from.height);
        const auto right = scale(cut.rect.x + cut.rect.width, to.width, from.width);
        const auto bottom = scale(cut.rect.y + cut.rect.height, to.height, from.height);
        cut.rect = Rect{left, top, std::max(right - left, 1), std::max(bottom - top, 1)};
    }
    return movedCuts(std::move(cuts), 0, 0, to);
}

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
    picture.meta.cuts = movedCuts(std::move(picture.meta.cuts), -token.rect.x, -token.rect.y, sizeOf(picture));
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
    const auto before = sizeOf(picture);
    if (auto failed = keep(picture.image, Transforms::Resize<Color::RGBA8888>(std::move(picture.image), wanted, token.filter))) {
        return failed;
    }
    picture.meta.cuts = scaledCuts(std::move(picture.meta.cuts), before, sizeOf(picture));
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

/** a grey stays grey, so the ones negate runs on as they are are not widened first */
std::optional<Error> negate(Picture& picture, const Token&) {
    picture.indexed.reset();
    if (picture.native) {
        auto& native = *picture.native;
        std::optional<Error> out;
        bool done = false;
        HAIO_FOR_EACH_COLOR(c) {
            constexpr Color colour = std::meta::extract<Color>(c);
            if constexpr (colour != Color::PALETTE && Transforms::Negatable<colour>) {
                if (!done && colour == native.color) {
                    done = true;
                    auto negated = Transforms::Negate<colour>(Image<colour>{native.width, native.height, std::move(native.data)});
                    if (negated) native.data = std::move(negated->data);
                    else out = negated.error();
                }
            }
        }
        if (done) return out;
    }
    if (auto failed = picture.rgba()) return failed;
    return keep(picture.image, Transforms::Negate<Color::RGBA8888>(std::move(picture.image)));
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

/** the picture in another colour, kept there as it is kept in the one it was decoded in */
std::optional<Error> pixFmt(Picture& picture, const Token& token) {
    // indices are only ever the ones a palette left, so they are kept rather than made
    if (token.color == Color::PALETTE) {
        if (picture.indexed) return std::nullopt;
        return Error{ErrorCode::InvalidInput,
                     "a palette is colours a picture was fitted to, and this one was not; name some with -palete"};
    }
    picture.indexed.reset();
    auto converted = Convert(picture.current(), *token.color);
    if (!converted) return converted.error();
    picture.native = *std::move(converted);
    return std::nullopt;
}

void blit(Image<Color::RGBA8888>& canvas, const Image<Color::RGBA8888>& picture, int left, int top) {
    const auto row = static_cast<size_t>(picture.width) * 4;
    for (int y = 0; y < picture.height; y++) {
        const auto to = (static_cast<size_t>(top + y) * static_cast<size_t>(canvas.width) + static_cast<size_t>(left)) * 4;
        std::copy_n(picture.data.data() + static_cast<size_t>(y) * row, row, canvas.data.data() + to);
    }
}

/**
 * the pictures laid on one canvas, which keeps where each of them went as its cuts.
 * one that already had cuts brings those rather than one of its own, so a grid of
 * grids is a single index of everything that went into them.
 */
Result<Picture> grid(std::span<Picture> pictures, const Token& token) {
    std::vector<Size> sizes;
    sizes.reserve(pictures.size());
    for (const auto& picture : pictures) sizes.push_back(sizeOf(picture));

    const Atlas::Spread spread{token.quantum, token.gap};
    auto layout = token.cells.width == 0 && token.cells.height == 0 ? Atlas::shelf(sizes, spread)
                                                                     : Atlas::grid(sizes, token.cells, spread);
    if (!layout) return std::unexpected(layout.error());

    Picture out;
    out.image = Image<Color::RGBA8888>{
        layout->canvas.width, layout->canvas.height,
        std::vector<uint8_t>(static_cast<size_t>(layout->canvas.width) * static_cast<size_t>(layout->canvas.height) * 4)};

    for (size_t at = 0; at < pictures.size(); at++) {
        auto& picture = pictures[at];
        if (auto failed = picture.rgba()) return std::unexpected(*failed);

        const auto& room = layout->rects[at];
        const auto [dx, dy] = placeOf(Size{room.width, room.height}, sizes[at], token.gravity, 0, 0);
        const auto left = room.x + dx;
        const auto top = room.y + dy;
        blit(out.image, picture.image, left, top);
        picture.image.data = {};

        if (picture.meta.cuts.empty()) {
            out.meta.cuts.push_back(Cut{Rect{left, top, sizes[at].width, sizes[at].height}, picture.meta.name.value_or(std::string{})});
        } else {
            auto moved = movedCuts(std::move(picture.meta.cuts), left, top, layout->canvas);
            out.meta.cuts.insert(out.meta.cuts.end(), std::make_move_iterator(moved.begin()), std::make_move_iterator(moved.end()));
        }
        if (!out.meta.name) out.meta.name = std::move(picture.meta.name);
    }
    return out;
}

/** a blend declared for the two colours as they are, so tint sees a grey as a grey */
std::optional<Result<Image<Color::RGBA8888>>> blendAsTheyAre(Compose compose, Picture& base, Picture& layer, int x, int y) {
    const auto colourOf = [](const Picture& picture) { return picture.native ? picture.native->color : Color::RGBA8888; };
    const auto under = colourOf(base);
    const auto over = colourOf(layer);

    std::optional<Result<Image<Color::RGBA8888>>> out;
    template for (constexpr auto m : std::define_static_array(std::meta::enumerators_of(^^Compose))) {
        constexpr Compose mode = std::meta::extract<Compose>(m);
        HAIO_FOR_EACH_COLOR(b) {
            constexpr Color bc = std::meta::extract<Color>(b);
            HAIO_FOR_EACH_COLOR(l) {
                constexpr Color lc = std::meta::extract<Color>(l);
                constexpr bool elsewhere = bc == Color::RGBA8888 && lc == Color::RGBA8888;
                if constexpr (bc != Color::PALETTE && lc != Color::PALETTE && !elsewhere
                              && Transforms::Blendable<mode, bc, lc>) {
                    if (!out && mode == compose && bc == under && lc == over) {
                        auto shape = base.current();
                        auto colour = layer.current();
                        base.native.reset();
                        layer.native.reset();
                        out = Transforms::Blend<mode, bc, lc>(Image<bc>{shape.width, shape.height, std::move(shape.data)},
                                                              Image<lc>{colour.width, colour.height, std::move(colour.data)},
                                                              x, y);
                    }
                }
            }
        }
    }
    return out;
}

/**
 * the layer over the base; the result is full colour, so the indices of a palette go.
 *
 * the base keeps its size, so it keeps its cuts, and the layer's join them moved to
 * where the layer landed: an atlas stays an atlas whichever side of the composite it
 * was on. the name is the base's, or the layer's when the base is a canvas painted
 * only to be drawn on.
 */
std::optional<Error> composite(Picture& base, Picture layer, Compose compose, Gravity gravity, int x, int y) {
    base.indexed.reset();
    const auto [left, top] = placeOf(sizeOf(base), sizeOf(layer), gravity, x, y);

    auto laid = movedCuts(std::move(layer.meta.cuts), left, top, sizeOf(base));
    base.meta.cuts.insert(base.meta.cuts.end(), std::make_move_iterator(laid.begin()), std::make_move_iterator(laid.end()));
    if (!base.meta.name) base.meta.name = std::move(layer.meta.name);
    if (auto blended = blendAsTheyAre(compose, base, layer, left, top)) return keep(base.image, *std::move(blended));
    // the base's colour picks the blend, so a layer in any other colour is widened to the rgba8888 they take
    if (layer.native && layer.native->color != Color::RGBA8888) {
        if (auto failed = layer.rgba()) return failed;
        if (auto blended = blendAsTheyAre(compose, base, layer, left, top)) return keep(base.image, *std::move(blended));
    }
    if (auto failed = base.rgba()) return failed;
    if (auto failed = layer.rgba()) return failed;
    return keep(base.image, Transforms::Composite<Color::RGBA8888>(std::move(base.image), layer.image, compose,
                                                                    Gravity::NorthWest, left, top));
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
 * both build. every Decode and every Generate puts a picture on the stack, a transform changes the one
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

    Format outputFormat = Format::RAW;
    std::optional<Color> outputColor;
    Settings encodeSettings;
    BodyEncoding outputEncoding = BodyEncoding::Identity;
    std::string outputName;

    /** the next input, decoded and put on top */
    auto decode = [&](const Settings& settings) -> std::optional<Error> {
        if (nextInput >= inputs.size()) return Error{ErrorCode::InvalidInput, "more pictures asked for than were given"};
        auto& input = inputs[nextInput++];
        auto decoded = DecodeNative(input, settings);
        if (!decoded) return decoded.error();
        stack.push_back(Picture{*std::move(decoded), {}, std::nullopt});
        if (!input.path.empty()) stack.back().meta.name = input.path;

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

            // painted rather than read, so it takes no input and leaves the next one
            // for the Decode it belongs to
            case TokenKind::Generate: {
                // without -size, "qr:foo xc:red" paints the red as big as the code
                auto settings = token.settings;
                if (!settingNamed(settings, "size") && sizeFallsBack(token.brush) && stack.size() > scopes.back()) {
                    const auto before = sizeOf(stack.back());
                    settings.push_back(Setting{"size", std::to_string(before.width) + "x" + std::to_string(before.height)});
                }
                auto painted = GenerateNative(token.brush, token.expression, settings);
                if (!painted) {
                    failure = painted.error();
                    break;
                }
                stack.push_back(Picture{*std::move(painted), {}, std::nullopt});
                break;
            }
            case TokenKind::Crop: failure = onTop(crop, token); break;
            case TokenKind::Resize: failure = onTop(resize, token); break;
            case TokenKind::Radius: failure = onTop(radius, token); break;
            case TokenKind::Negate: failure = onTop(negate, token); break;
            case TokenKind::Palette: failure = onTop(palette, token); break;
            case TokenKind::PixFmt: failure = onTop(pixFmt, token); break;

            case TokenKind::Composite: {
                auto pictures = inScope();
                if (!pictures) {
                    failure = pictures.error();
                    break;
                }
                if (pictures->size() < 2 || pictures->size() > 4) {
                    failure = Error{ErrorCode::InvalidInput, "-composite joins a picture and one to three over it, and there are "
                                                                 + std::to_string(pictures->size())};
                    break;
                }
                // three over the destination are one: the first tints the shape and the second
                // its negative, and the first of the two goes over the second
                if (pictures->size() == 4) {
                    auto second = std::move(stack.back());
                    stack.pop_back();
                    auto first = std::move(stack.back());
                    stack.pop_back();
                    // a copy of the destination, so its cuts would otherwise come back twice
                    auto negative = stack.back();
                    negative.meta.cuts.clear();
                    failure = negate(negative, token);
                    if (!failure) failure = composite(negative, std::move(second), Compose::Tint, Gravity::NorthWest, 0, 0);
                    if (!failure) failure = composite(stack.back(), std::move(first), Compose::Tint, Gravity::NorthWest, 0, 0);
                    if (!failure) failure = composite(negative, std::move(stack.back()), Compose::Over, Gravity::NorthWest, 0, 0);
                    if (failure) break;
                    stack.back() = std::move(negative);
                }
                // two over the destination are one: the second tints the first, from its top left corner
                if (pictures->size() == 3) {
                    auto colour = std::move(stack.back());
                    stack.pop_back();
                    failure = composite(stack.back(), std::move(colour), Compose::Tint, Gravity::NorthWest, 0, 0);
                    if (failure) break;
                }
                auto layer = std::move(stack.back());
                stack.pop_back();
                failure = composite(stack.back(), std::move(layer), token.compose, token.gravity,
                                    token.rect.x, token.rect.y);
                break;
            }

            case TokenKind::Grid: {
                auto pictures = inScope();
                if (!pictures) {
                    failure = pictures.error();
                    break;
                }
                auto laid = grid(*pictures, token);
                if (!laid) {
                    failure = laid.error();
                    break;
                }
                stack.resize(scopes.back());
                stack.push_back(*std::move(laid));
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

            case TokenKind::Deliver:
                outputEncoding = token.encoding;
                if (!token.name.empty()) outputName = token.name;
                break;
        }
        if (failure) return std::unexpected(*failure);
    }

    auto finish = [&](Blob blob) -> Result<Blob> {
        if (!outputName.empty()) blob.path = outputName;
        if (outputEncoding != BodyEncoding::Identity) blob.data = encodeBody(blob.data, outputEncoding);
        return blob;
    };

    /**
     * no container asked for and nothing decoded: the cdn's untouched file, which is
     * what a pipeline with nothing to do hands back.
     */
    if (outputFormat == Format::RAW && !outputColor && stack.empty() && nextInput == 0 && inputs.size() == 1) {
        return finish(std::move(inputs.front()));
    }

    if (auto failed = decodeLazily()) return std::unexpected(*failed);

    if (stack.size() != 1) {
        return std::unexpected(Error{ErrorCode::InvalidInput, std::to_string(stack.size())
                                                                  + " pictures and one output; join them with -composite or -grid"});
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

        return finish(Blob{Format::RAW, to, std::string(contentTypeFor(Format::RAW)),
                           picture.meta.name.value_or(std::string{}), std::move(pixels->data),
                           Size{pixels->width, pixels->height}});
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
                             : Encode(picture.current(), outputFormat, outputColor, encodeSettings, picture.meta);
    if (!encoded) return encoded;
    encoded->path = picture.meta.name.value_or(std::string{});
    return finish(*std::move(encoded));
}

}

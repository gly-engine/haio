#pragma once

// the generated list comes first on purpose. everything below asks the codecs what
// they can do, and a concept evaluated before they are declared caches the answer
// "no" for the rest of the build, silently.
#include <haio/generated/codec.hpp>

#include "haio_codec.hpp"
#include "haio_convert.hpp"

namespace Haio {

constexpr std::string_view formatName(Format wanted) {
    HAIO_FOR_EACH_FORMAT(e) {
        if (std::meta::extract<Format>(e) == wanted) return lowerOf(e);
    }
    return "raw";
}

constexpr std::string_view colorName(Color wanted) {
    HAIO_FOR_EACH_COLOR(e) {
        if (std::meta::extract<Color>(e) == wanted) return lowerOf(e);
    }
    return "rgba8888";
}

namespace Detail {

constexpr std::string_view lowered(std::string_view name, char (&buffer)[32]) {
    if (name.size() >= sizeof(buffer)) return {};
    for (size_t i = 0; i < name.size(); i++) {
        const char c = name[i];
        buffer[i] = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    }
    return {buffer, name.size()};
}

}

constexpr Format formatFromName(std::string_view name) {
    char buffer[32]{};
    const auto key = Detail::lowered(name, buffer);
    HAIO_FOR_EACH_FORMAT(e) {
        if (lowerOf(e) == key) return std::meta::extract<Format>(e);
    }
    return Format::RAW;
}

/**
 * the colour somebody typed, or nothing when no colour answers to that name.
 *
 * the enumerator names come first and then colorAliases, which is every other name
 * the same layouts go by. borrowing ffmpeg's option without its vocabulary would be
 * borrowing the half that does not help.
 */
constexpr std::optional<Color> colorNamed(std::string_view name) {
    char buffer[32]{};
    const auto key = Detail::lowered(name, buffer);
    HAIO_FOR_EACH_COLOR(e) {
        if (lowerOf(e) == key) return std::meta::extract<Color>(e);
    }

    for (const auto& alias : colorAliases) {
        if (alias.spelling == key) return alias.color;
    }
    return std::nullopt;
}

/** the same question where an unknown name is not worth stopping for */
constexpr Color colorFromName(std::string_view name) {
    const auto color = colorNamed(name);
    return color ? *color : Color::RGBA8888;
}

/** what a codec reads off the command line, for a format named at runtime */
constexpr const Codecs::Reads& readsOf(Format wanted) {
    HAIO_FOR_EACH_FORMAT(e) {
        constexpr Format format = std::meta::extract<Format>(e);
        if (format == wanted) return Codecs::reads<format>;
    }
    return Codecs::reads<Format::RAW>;
}

constexpr std::string_view brushName(Brush wanted) {
    HAIO_FOR_EACH_BRUSH(e) {
        if (std::meta::extract<Brush>(e) == wanted) return kebabOf(e);
    }
    return "xc";
}

/** the brush a prefix names, by its own name or one it declared, in any case */
constexpr std::optional<Brush> brushNamed(std::string_view name) {
    char buffer[32]{};
    const auto key = Detail::lowered(name, buffer);
    HAIO_FOR_EACH_BRUSH(e) {
        constexpr Brush brush = std::meta::extract<Brush>(e);
        if (kebabOf(e) == key) return brush;
        for (const auto alias : Codecs::draws<brush>.aliases) {
            if (!alias.empty() && alias == key) return brush;
        }
    }
    return std::nullopt;
}


/** what a file turned out to be: the container and the colour it holds */
struct Found {
    Format format = Format::RAW;
    Color color = Color::RGBA8888;

    explicit operator bool() const noexcept { return format != Format::RAW; }
};

/** asks every declared pair, so "which payload is inside" needs no separate reader */
inline Found Detect(Bytes data) {
    Found found;
    HAIO_FOR_EACH_FORMAT(f) {
        constexpr Format format = std::meta::extract<Format>(f);
        HAIO_FOR_EACH_COLOR(c) {
            constexpr Color color = std::meta::extract<Color>(c);
            if constexpr (Codecs::Detectable<format, color>) {
                if (!found && Codecs::Detect<format, color>(data)) found = Found{format, color};
            }
        }
    }
    return found;
}

constexpr bool detectable(Format format, Color color) {
    HAIO_FOR_EACH_FORMAT(f) {
        constexpr Format ff = std::meta::extract<Format>(f);
        HAIO_FOR_EACH_COLOR(c) {
            constexpr Color cc = std::meta::extract<Color>(c);
            if (ff == format && cc == color) return Codecs::Detectable<ff, cc>;
        }
    }
    return false;
}

constexpr bool encodable(Format format, Color color) {
    HAIO_FOR_EACH_FORMAT(f) {
        constexpr Format ff = std::meta::extract<Format>(f);
        HAIO_FOR_EACH_COLOR(c) {
            constexpr Color cc = std::meta::extract<Color>(c);
            if (ff == format && cc == color) return Codecs::Encodable<ff, cc>;
        }
    }
    return false;
}

/**
 * the colour a container is written in when nobody names one: the one it declares as
 * its own, and failing that the first it can write at all.
 *
 * before this there was only the second half, so the colour a file came out in was
 * whichever one happened to be declared earliest in the enum. that is fine while a
 * container writes one colour and arbitrary the moment it writes six.
 */
constexpr std::optional<Color> encodeColorFor(Format to) {
    std::optional<Color> first;
    HAIO_FOR_EACH_FORMAT(f) {
        constexpr Format format = std::meta::extract<Format>(f);
        if (format != to) continue;
        HAIO_FOR_EACH_COLOR(c) {
            constexpr Color color = std::meta::extract<Color>(c);
            if constexpr (Codecs::Encodable<format, color>) {
                if constexpr (Codecs::HasDefaultColor<format>) {
                    if (Codecs::DefaultColor<format>::value == color) return color;
                }
                if (!first) first = color;
            }
        }
    }
    return first;
}

/**
 * an image whose colour is a value rather than a type, which is what the runtime path
 * holds between a decode and an encode. it is the same thing as an Image<P> with the
 * P moved to where a string from a command line can reach it.
 *
 * a palette never travels like this: its entries have nowhere to go, so a picture
 * that decodes to one is expanded on the way in, exactly as it was before.
 */
struct AnyImage {
    Color color = Color::RGBA8888;
    int width = 0;
    int height = 0;
    std::vector<uint8_t> data;
};

/**
 * the same picture in another colour, both named at runtime.
 *
 * the pair that goes straight there wins, and only a colour with no road of its own
 * goes through rgba8888. before this everything went through it, so a png of rgb
 * became rgba to become yuv, with a whole alpha plane written and read for nothing.
 */
inline Result<AnyImage> Convert(AnyImage image, Color to) {
    if (image.color == to) return image;

    const auto from = image.color;
    Result<AnyImage> out =
        std::unexpected(Error{ErrorCode::UnsupportedFormat,
                              "no route from " + std::string(colorName(from)) + " to " + std::string(colorName(to))});
    bool done = false;

    HAIO_FOR_EACH_COLOR(f) {
        constexpr Color source = std::meta::extract<Color>(f);
        if constexpr (source != Color::PALETTE) {
            if (done || source != from) continue;

            HAIO_FOR_EACH_COLOR(t) {
                constexpr Color target = std::meta::extract<Color>(t);
                if constexpr (Codecs::Convertible<source, target>) {
                    if (target != to) continue;

                    auto converted = Codecs::Convert<source, target>(
                        Image<source>{image.width, image.height, std::move(image.data)});
                    if (!converted) {
                        out = std::unexpected(converted.error());
                    } else {
                        out = AnyImage{target, converted->width, converted->height, std::move(converted->data)};
                    }
                    done = true;
                }
            }
        }
    }

    if (done || from == Color::RGBA8888 || to == Color::RGBA8888) return out;

    HAIO_TRY(middle, Convert(std::move(image), Color::RGBA8888));
    return Convert(std::move(middle), to);
}

/**
 * a picture painted by a brush named at runtime, in the first colour it paints in.
 * that is rgba8888 for all of them today; a brush that learns to paint straight into
 * another colour only has to be declared for it.
 */
inline Result<AnyImage> GenerateNative(Brush brush, std::string_view words, const Settings& settings) {
    Result<AnyImage> out = std::unexpected(Error{ErrorCode::UnsupportedFormat,
                                                 "this build cannot paint " + std::string(brushName(brush))});
    bool done = false;
    HAIO_FOR_EACH_BRUSH(b) {
        constexpr Brush painter = std::meta::extract<Brush>(b);
        HAIO_FOR_EACH_COLOR(c) {
            constexpr Color color = std::meta::extract<Color>(c);
            if constexpr (Codecs::Generatable<painter, color> && color != Color::PALETTE) {
                if (done || painter != brush) continue;
                done = true;
                auto painted = Codecs::Generate<painter, color>(words, settings);
                out = painted ? Result<AnyImage>{AnyImage{color, painted->width, painted->height, std::move(painted->data)}}
                              : Result<AnyImage>{std::unexpected(painted.error())};
            }
        }
    }
    return out;
}

/**
 * the runtime way in: reads the bytes and routes to the pair that matched, keeping
 * the colour the file was stored in so that whatever comes next can start from it.
 */
inline Result<AnyImage> DecodeNative(const Blob& blob, const Settings& settings = {}) {
    const auto found = blob.format != Format::RAW ? Found{blob.format, blob.color} : Detect(blob.data);
    if (!found) {
        return std::unexpected(Error{ErrorCode::UnsupportedFormat, "unrecognised input"});
    }

    Result<AnyImage> out =
        std::unexpected(Error{ErrorCode::UnsupportedFormat,
                              "cannot decode " + std::string(formatName(found.format))
                                  + " " + std::string(colorName(found.color))});

    HAIO_FOR_EACH_FORMAT(f) {
        constexpr Format format = std::meta::extract<Format>(f);
        HAIO_FOR_EACH_COLOR(c) {
            constexpr Color color = std::meta::extract<Color>(c);
            if constexpr (Codecs::Decodable<format, color>) {
                if (format != found.format || color != found.color) continue;

                auto decoded = Codecs::decode<format, color>(blob, settings);
                if (!decoded) {
                    out = std::unexpected(decoded.error());
                } else if constexpr (color != Color::PALETTE) {
                    out = AnyImage{color, decoded->width, decoded->height, std::move(decoded->data)};
                } else {
                    auto expanded = Codecs::Convert<Color::PALETTE, Color::RGBA8888>(*std::move(decoded));
                    if (!expanded) {
                        out = std::unexpected(expanded.error());
                    } else {
                        out = AnyImage{Color::RGBA8888, expanded->width, expanded->height, std::move(expanded->data)};
                    }
                }
            }
        }
    }
    return out;
}

/** the same way in, for a caller that wants one type to hold whatever the file was */
inline Result<Image<Color::RGBA8888>> Decode(const Blob& blob) {
    HAIO_TRY(native, DecodeNative(blob));
    HAIO_TRY(rgba, Convert(std::move(native), Color::RGBA8888));
    return Image<Color::RGBA8888>{rgba.width, rgba.height, std::move(rgba.data)};
}

/**
 * the same picture in another colour, named at runtime.
 *
 * it hands back bytes rather than an image because at runtime there is no Image<P> to
 * hand back: the colour is a value here, and the type it would pick is a compile time
 * thing. the callers that want one already know which they want and say so through
 * Codecs::Convert.
 */
inline Result<std::vector<uint8_t>> Convert(const Image<Color::RGBA8888>& image, Color to) {
    HAIO_TRY(converted, Convert(AnyImage{Color::RGBA8888, image.width, image.height, image.data}, to));
    return std::move(converted.data);
}

/**
 * the runtime way out. the colour stored inside is the one asked for, or the one the
 * container calls its own when nobody asked, and the image gets there from whatever
 * colour it is in by the shortest road Convert knows.
 *
 * naming it is what -pix_fmt does, and it is the only way to say "a ktx2 holding
 * etc1" or "a tga holding bgr888" from a command line: the pair was always there in
 * Codecs::Encode, with nothing but this bridge between it and a string.
 */
inline Result<Blob> Encode(AnyImage image, Format to, std::optional<Color> as = std::nullopt,
                           const Settings& settings = {}) {
    const auto wanted = as ? as : encodeColorFor(to);
    if (!wanted) {
        return std::unexpected(Error{ErrorCode::UnsupportedFormat,
                                     "cannot encode " + std::string(formatName(to))});
    }

    Result<Blob> out = std::unexpected(Error{ErrorCode::UnsupportedFormat,
                                             "a " + std::string(formatName(to)) + " cannot hold "
                                                 + std::string(colorName(*wanted))});
    bool done = false;

    HAIO_FOR_EACH_COLOR(c) {
        constexpr Color color = std::meta::extract<Color>(c);
        HAIO_FOR_EACH_FORMAT(f) {
            constexpr Format format = std::meta::extract<Format>(f);
            if constexpr (Codecs::Encodable<format, color> && color != Color::PALETTE) {
                if (done || format != to || color != *wanted) continue;
                done = true;

                // the pair exists and the road to it may not, which is a different
                // sentence from the container not holding that colour at all
                auto converted = Convert(std::move(image), color);
                out = converted ? Codecs::encode<format, color>(
                                      Image<color>{converted->width, converted->height, std::move(converted->data)},
                                      settings)
                                : Result<Blob>{std::unexpected(converted.error())};
            }
        }
    }
    return out;
}

/** the same way out, for a caller holding rgba8888 */
inline Result<Blob> Encode(Image<Color::RGBA8888> image, Format to, std::optional<Color> as = std::nullopt) {
    return Encode(AnyImage{Color::RGBA8888, image.width, image.height, std::move(image.data)}, to, as);
}

/**
 * the indexed way out, for the containers that store a palette rather than expanding
 * it. there is no conversion into it here on purpose: haio does not pick colours for
 * anybody, so the picture arrives already fitted to a palette or not at all.
 */
inline Result<Blob> Encode(Image<Color::PALETTE> image, Format to, const Settings& settings = {}) {
    Result<Blob> out = std::unexpected(Error{ErrorCode::UnsupportedFormat,
                                             "a " + std::string(formatName(to)) + " cannot hold a palette"});
    HAIO_FOR_EACH_FORMAT(f) {
        constexpr Format format = std::meta::extract<Format>(f);
        if constexpr (Codecs::Encodable<format, Color::PALETTE>) {
            if (format != to) continue;
            out = Codecs::encode<format, Color::PALETTE>(std::move(image), settings);
        }
    }
    return out;
}

}

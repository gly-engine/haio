#pragma once

// the generated list comes first on purpose. everything below asks the codecs what
// they can do, and a concept evaluated before they are declared caches the answer
// "no" for the rest of the build, silently.
#include "haio_codecs.hpp"

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
 * the runtime way in: reads the bytes, routes to the pair that matched, and brings
 * the result to rgba8888 so the caller has one type to hold.
 *
 * @todo everything funnels through rgba8888, so ktx2(etc1) -> ktx2 decompresses and
 * recompresses for nothing. keeping the payload needs a typed path, which is exactly
 * what Codecs::Decode<Format, Color> already offers; only this runtime bridge flattens it.
 */
inline Result<Image<Color::RGBA8888>> Decode(const Blob& blob) {
    const auto found = blob.format != Format::RAW ? Found{blob.format, blob.color} : Detect(blob.data);
    if (!found) {
        return std::unexpected(Error{ErrorCode::UnsupportedFormat, "unrecognised input"});
    }

    Result<Image<Color::RGBA8888>> out =
        std::unexpected(Error{ErrorCode::UnsupportedFormat,
                              "cannot decode " + std::string(formatName(found.format))
                                  + " " + std::string(colorName(found.color))});

    HAIO_FOR_EACH_FORMAT(f) {
        constexpr Format format = std::meta::extract<Format>(f);
        HAIO_FOR_EACH_COLOR(c) {
            constexpr Color color = std::meta::extract<Color>(c);
            if constexpr (Codecs::Decodable<format, color>) {
                if (format != found.format || color != found.color) continue;

                auto decoded = Codecs::Decode<format, color>(blob);
                if (!decoded) {
                    out = std::unexpected(decoded.error());
                } else if constexpr (color == Color::RGBA8888) {
                    out = *std::move(decoded);
                } else if constexpr (Codecs::Convertible<color, Color::RGBA8888>) {
                    out = Codecs::Convert<color, Color::RGBA8888>(*std::move(decoded));
                } else {
                    out = std::unexpected(Error{ErrorCode::UnsupportedFormat,
                                                "no route from " + std::string(colorName(color)) + " to rgba8888"});
                }
            }
        }
    }
    return out;
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
    if (to == Color::RGBA8888) return image.data;

    Result<std::vector<uint8_t>> out =
        std::unexpected(Error{ErrorCode::UnsupportedFormat,
                              "no route from rgba8888 to " + std::string(colorName(to))});

    HAIO_FOR_EACH_COLOR(c) {
        constexpr Color color = std::meta::extract<Color>(c);
        if constexpr (Codecs::Convertible<Color::RGBA8888, color>) {
            if (color != to) continue;

            auto converted = Codecs::Convert<Color::RGBA8888, color>(image);
            if (!converted) {
                out = std::unexpected(converted.error());
            } else {
                out = std::move(converted->data);
            }
        }
    }
    return out;
}

/**
 * the runtime way out. rgba8888 goes in because that is what the runtime decode
 * hands back; the colour stored inside is the one asked for, or the one the container
 * calls its own when nobody asked.
 *
 * naming it is what -pix_fmt does, and it is the only way to say "a ktx2 holding
 * etc1" or "a tga holding bgr888" from a command line: the pair was always there in
 * Codecs::Encode, with nothing but this bridge between it and a string.
 */
inline Result<Blob> Encode(Image<Color::RGBA8888> image, Format to, std::optional<Color> as = std::nullopt) {
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
            if constexpr (Codecs::Encodable<format, color>) {
                if (done || format != to || color != *wanted) continue;

                if constexpr (color == Color::RGBA8888) {
                    out = Codecs::Encode<format, color>(image);
                    done = true;
                } else if constexpr (Codecs::Convertible<Color::RGBA8888, color>) {
                    auto converted = Codecs::Convert<Color::RGBA8888, color>(image);
                    out = converted ? Codecs::Encode<format, color>(*std::move(converted))
                                    : Result<Blob>{std::unexpected(converted.error())};
                    done = true;
                } else {
                    // the pair exists and the road to it does not, which is a different
                    // sentence from the container not holding that colour at all
                    out = std::unexpected(Error{ErrorCode::UnsupportedFormat,
                                                "no route from rgba8888 to " + std::string(colorName(color))});
                    done = true;
                }
            }
        }
    }
    return out;
}

/**
 * the indexed way out, for the containers that store a palette rather than expanding
 * it. there is no conversion into it here on purpose: haio does not pick colours for
 * anybody, so the picture arrives already fitted to a palette or not at all.
 */
inline Result<Blob> Encode(Image<Color::PALETTE> image, Format to) {
    Result<Blob> out = std::unexpected(Error{ErrorCode::UnsupportedFormat,
                                             "a " + std::string(formatName(to)) + " cannot hold a palette"});
    HAIO_FOR_EACH_FORMAT(f) {
        constexpr Format format = std::meta::extract<Format>(f);
        if constexpr (Codecs::Encodable<format, Color::PALETTE>) {
            if (format != to) continue;
            out = Codecs::Encode<format, Color::PALETTE>(std::move(image));
        }
    }
    return out;
}

}

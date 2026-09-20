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

constexpr Color colorFromName(std::string_view name) {
    char buffer[32]{};
    const auto key = Detail::lowered(name, buffer);
    HAIO_FOR_EACH_COLOR(e) {
        if (lowerOf(e) == key) return std::meta::extract<Color>(e);
    }
    // short spellings the enumerator name cannot carry until enum annotations land
    if (key == "rgba") return Color::RGBA8888;
    if (key == "rgb") return Color::RGB888;
    return Color::RGBA8888;
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
 * the runtime way out. rgba8888 goes in because that is what the runtime decode
 * hands back; the colour stored inside is whichever one the format can take.
 *
 * @todo the payload colour is picked here rather than asked for, so there is no way
 * to say "a ktx2 holding etc1" from the command line yet.
 */
inline Result<Blob> Encode(Image<Color::RGBA8888> image, Format to) {
    Result<Blob> out = std::unexpected(Error{ErrorCode::UnsupportedFormat,
                                             "cannot encode " + std::string(formatName(to))});
    bool done = false;

    HAIO_FOR_EACH_COLOR(c) {
        constexpr Color color = std::meta::extract<Color>(c);
        HAIO_FOR_EACH_FORMAT(f) {
            constexpr Format format = std::meta::extract<Format>(f);
            if constexpr (Codecs::Encodable<format, color>) {
                if (done || format != to) continue;

                if constexpr (color == Color::RGBA8888) {
                    out = Codecs::Encode<format, color>(image);
                    done = true;
                } else if constexpr (Codecs::Convertible<Color::RGBA8888, color>) {
                    auto converted = Codecs::Convert<Color::RGBA8888, color>(image);
                    out = converted ? Codecs::Encode<format, color>(*std::move(converted))
                                    : Result<Blob>{std::unexpected(converted.error())};
                    done = true;
                }
            }
        }
    }
    return out;
}

}

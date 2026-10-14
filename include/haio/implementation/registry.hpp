#pragma once

// the generated list comes first on purpose. everything below asks the codecs what
// they can do, and a concept evaluated before they are declared caches the answer
// "no" for the rest of the build, silently.
#include <haio/generated/codecs.hpp>

#include "haio/implementation/codecs.hpp"
#include "haio/implementation/codecs/convert.hpp"

#include <optional>
#include <string>
#include <string_view>

/**
 * the codecs asked at runtime: a format, a colour or a brush named by a string, and
 * the pair that answers found by walking every one this build declared. this is what
 * stands between a command line and Codecs::Decode<Format::PNG, Color::RGB888>.
 */
namespace Haio::Implementation {

/** the format a path's extension names, or RAW when none does */
Format formatFromExtension(std::string_view path);

/** the format a mime type names, or RAW when none does */
Format formatFromContentType(std::string_view contentType);

/** the extension haio writes a format with, the first its enumerator declares */
std::string_view extensionFor(Format format);

/** the mime type haio sends a format as, the first its enumerator declares */
std::string_view contentTypeFor(Format format);

constexpr std::string_view formatName(Format wanted) {
    HAIO_FOR_EACH_FORMAT(e) {
        if (std::meta::extract<Format>(e) == wanted) return Util::lowerOf(e);
    }
    return "raw";
}

constexpr std::string_view colorName(Color wanted) {
    HAIO_FOR_EACH_COLOR(e) {
        if (std::meta::extract<Color>(e) == wanted) return Util::lowerOf(e);
    }
    return "rgba8888";
}

constexpr std::string_view lowered(std::string_view name, char (&buffer)[32]) {
    if (name.size() >= sizeof(buffer)) return {};
    for (size_t i = 0; i < name.size(); i++) {
        const char c = name[i];
        buffer[i] = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    }
    return {buffer, name.size()};
}


constexpr Format formatFromName(std::string_view name) {
    char buffer[32]{};
    const auto key = lowered(name, buffer);
    HAIO_FOR_EACH_FORMAT(e) {
        if (Util::lowerOf(e) == key) return std::meta::extract<Format>(e);
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
    const auto key = lowered(name, buffer);
    HAIO_FOR_EACH_COLOR(e) {
        if (Util::lowerOf(e) == key) return std::meta::extract<Color>(e);
    }

    for (const auto& alias : Trash::colorAliases) {
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

constexpr std::string_view brushName(Codecs::Brush wanted) {
    HAIO_FOR_EACH_BRUSH(e) {
        if (std::meta::extract<Codecs::Brush>(e) == wanted) return Util::kebabOf(e);
    }
    return "xc";
}

/** the brush a prefix names, by its own name or one it declared, in any case */
constexpr std::optional<Codecs::Brush> brushNamed(std::string_view name) {
    char buffer[32]{};
    const auto key = lowered(name, buffer);
    HAIO_FOR_EACH_BRUSH(e) {
        constexpr Codecs::Brush brush = std::meta::extract<Codecs::Brush>(e);
        if (Util::kebabOf(e) == key) return brush;
        for (const auto alias : Codecs::draws<brush>.aliases) {
            if (!alias.empty() && alias == key) return brush;
        }
    }
    return std::nullopt;
}

/**
 * whether a brush's -size, when nobody says, is only a stand-in -- xc:'s 1x1 -- rather
 * than a size of its own, as a qr: has. a stand-in gives way to the picture before it.
 */
constexpr bool sizeFallsBack(Codecs::Brush wanted) {
    HAIO_FOR_EACH_BRUSH(e) {
        constexpr Codecs::Brush brush = std::meta::extract<Codecs::Brush>(e);
        if (brush != wanted) continue;
        for (const auto& option : Codecs::draws<brush>.options) {
            if (option.name() == "size") return !option.fallback.empty();
        }
    }
    return false;
}

/** whether a brush first of three in a -composite is its shape, declared as Draws::composite4 */
constexpr bool composite4Of(Codecs::Brush wanted) {
    HAIO_FOR_EACH_BRUSH(e) {
        constexpr Codecs::Brush brush = std::meta::extract<Codecs::Brush>(e);
        if (brush == wanted) return Codecs::draws<brush>.composite4;
    }
    return false;
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
                if constexpr (WIP::HasDefaultColor<format>) {
                    if (WIP::DefaultColor<format>::value == color) return color;
                }
                if (!first) first = color;
            }
        }
    }
    return first;
}

/** the same way in as Decode<>, for a caller that wants one type to hold whatever the file was */
Result<Image<Color::RGBA8888>> decode(const Blob& blob);

}

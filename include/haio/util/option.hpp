#pragma once

#include "haio/implementation/error.hpp"
#include "haio/implementation/image.hpp"
#include "haio/implementation/option.hpp"

#include <charconv>
#include <meta>
#include <optional>
#include <string>
#include <string_view>

/**
 * reading what an Option was handed: whether it fits, what it says in words, and
 * the whole number or the enumerator it spells. the command line checks with these
 * the moment a line is read, and a codec checks again with the same ones for
 * whatever else hands it settings.
 */
namespace Haio::Util {

using namespace Implementation;

/**
 * two names the same once case, spaces, dashes and underscores stop mattering:
 * "Code 128", "CODE-128" and "code128" are one symbology however somebody wrote it,
 * and "north_east" the same gravity as "NorthEast".
 */
constexpr bool sameName(std::string_view a, std::string_view b) {
    const auto skipped = [](char c) { return c == ' ' || c == '-' || c == '_'; };
    const auto lower = [](char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; };
    size_t i = 0;
    size_t j = 0;
    while (true) {
        while (i < a.size() && skipped(a[i])) i++;
        while (j < b.size() && skipped(b[j])) j++;
        if (i == a.size() || j == b.size()) return i == a.size() && j == b.size();
        if (lower(a[i++]) != lower(b[j++])) return false;
    }
}

constexpr std::optional<int> wholeNumber(std::string_view text) {
    int value = 0;
    const auto [end, problem] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (problem != std::errc{} || end != text.data() + text.size()) return std::nullopt;
    return value;
}

/**
 * the enumerator a name spells, in any case, or nothing: the one reader for every
 * enum a command line names -- dithers, limits, resize filters -- so what the parser
 * accepts and what the stage understands are the same list by construction.
 */
template <typename E>
constexpr std::optional<E> enumNamed(std::string_view name) {
    template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^E))) {
        if (Util::sameName(std::meta::identifier_of(e), name)) return std::meta::extract<E>(e);
    }
    return std::nullopt;
}

/** "from 1 to 100", "one of point, box", "WxH": what the shape asks for, in words */
inline std::string shapeOf(const Option& option) {
    switch (option.shape) {
        case OptionShape::Any: break;
        case OptionShape::Integer:
            return "a whole number from " + std::to_string(option.least) + " to " + std::to_string(option.most);
        case OptionShape::Size: return "WxH";
        case OptionShape::Name: {
            std::string out = option.names.size() == 1 ? "" : "one of ";
            for (size_t i = 0; i < option.names.size(); i++) {
                if (i) out += i + 1 == option.names.size() ? " or " : ", ";
                out += option.names[i];
            }
            return out;
        }
    }
    return {};
}

/** the help, and after it what the value has to be and what it is when nobody says */
inline std::string describe(const Option& option) {
    std::string out{option.help};
    if (const auto shape = shapeOf(option); !shape.empty()) out += (out.empty() ? "" : "; ") + shape;
    if (!option.fallback.empty()) out += ", " + std::string(option.fallback) + " when nobody says";
    return out;
}

/** a word as imagemagick quotes it in a message, `like this' */
inline std::string quoted(std::string_view word) {
    return "`" + std::string(word) + "'";
}

/**
 * nothing when the value fits the option, and why not when it does not, worded the
 * way imagemagick words it so the two read alike: "invalid argument for option
 * `-quality': 0", or "unrecognized gravity type `up'" for a name that is not in the list.
 */
inline std::optional<std::string> refusal(const Option& option, std::string_view value) {
    const auto why = [&] {
        if (option.shape == OptionShape::Name && !option.called.empty()) {
            return "unrecognized " + std::string(option.called) + " " + Util::quoted(value);
        }
        return "invalid argument for option " + Util::quoted("-" + std::string(option.name())) + ": " + std::string(value);
    };

    switch (option.shape) {
        case OptionShape::Any: return std::nullopt;
        case OptionShape::Integer: {
            const auto number = Util::wholeNumber(value);
            if (!number || *number < option.least || *number > option.most) return why();
            return std::nullopt;
        }
        case OptionShape::Size: {
            const auto x = value.find_first_of("xX");
            if (x == std::string_view::npos) return why();
            const auto width = Util::wholeNumber(value.substr(0, x));
            const auto height = Util::wholeNumber(value.substr(x + 1));
            if (!width || !height || *width <= 0 || *height <= 0) return why();
            return std::nullopt;
        }
        case OptionShape::Name:
            for (const auto name : option.names) {
                if (Util::sameName(std::string_view{name}, value)) return std::nullopt;
            }
            return why();
    }
    return std::nullopt;
}

inline const Setting* settingNamed(const Settings& settings, std::string_view name) {
    for (const auto& setting : settings) {
        if (setting.name == name) return &setting;
    }
    return nullptr;
}

/**
 * a whole number, read by the declaration that asked for it: its name, its bounds and
 * what it is when nobody says all come from the one Option, so a codec cannot
 * disagree with its own help text. the command line already checked it; this checks
 * again for whatever else hands settings in.
 */
inline Result<int> settingInt(const Settings& settings, const Option& option) {
    const auto* setting = settingNamed(settings, option.name());
    const std::string_view text = setting ? std::string_view{setting->value} : option.fallback;
    if (const auto why = refusal(option, text)) return std::unexpected(Error{ErrorCode::InvalidInput, *why});
    return *Util::wholeNumber(text);
}

}

#pragma once

#include <array>
#include <charconv>
#include <expected>
#include <meta>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/**
 * a stage is one thing the convert line can do: read a picture, change one, or write
 * one out. a transform is declared next to what it does, in include/haio/transforms/,
 * and nothing else has to list it; reading and writing belong to the codecs, which
 * declare what they take in include/haio/codecs/.
 *
 * the declaration is only data -- spellings, what it takes, which options it reads --
 * so the grammar can be printed from it without linking anything. turning the words
 * into a Token is build<&stage>, declared beside it and defined in its own source,
 * which only the parser ever instantiates.
 */
namespace Haio {

struct Token;

namespace Stages {

/**
 * what an option's value has to look like, checked the moment the line is read.
 *
 * it is data rather than a function so the grammar and the usage can say it too:
 * "from 1 to 100", "one of point", "WxH". the few shapes that come up over and over
 * are here once; anything stranger is Any, and the stage that reads it says no.
 */
enum class Shape {
    Any,       /**< whatever the stage makes of it */
    Integer,   /**< a whole number from least to most */
    Size,      /**< WxH, both sides positive */
    Name,      /**< one of names, in any case */
};

/**
 * the names of an enum's enumerators, lowered, for a Shape::Name option: a new
 * enumerator is a new name the line accepts, with nothing else to update. there are
 * no aliases on purpose -- one name per thing is the whole point of asking the enum.
 */
template <typename E>
consteval std::span<const char* const> namesOf() {
    // a pointer and not a string_view, because only a structural type can be made static
    std::vector<const char*> out;
    for (const auto e : std::meta::enumerators_of(^^E)) {
        std::string name{std::meta::identifier_of(e)};
        for (auto& c : name) {
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        }
        out.push_back(std::define_static_string(name));
    }
    return std::define_static_array(out);
}

/**
 * a setting written before the stage that uses it, as in "-filter bayer -palette cga".
 *
 * options are not known to the parser: any "-name value" that is not a stage is kept
 * until the next stage, and that stage says which ones it wants. whatever it does not
 * want is an error, because it was written in the belief that it would do something.
 */
struct Option {
    /** without the dash; the first is the one the grammar and the usage show */
    std::array<std::string_view, 2> spellings;

    /** the rule its value has to match, in the grammar */
    std::string_view takes;

    /** a stage refuses to run without it, as a qr code refuses without a -size */
    bool required = false;

    /** it may be written more than once, as -define is */
    bool repeats = false;

    std::string_view help;

    Shape shape = Shape::Any;

    /** Integer: the bounds, both included */
    int least = 0;
    int most = 0;

    /** Name: every name it answers to, from namesOf */
    std::span<const char* const> names = {};

    /** what it is when nobody says, as it would be written; empty when that is nothing */
    std::string_view fallback = {};

    /**
     * Name: what a name of this list is, for the message when one is not, which is
     * how imagemagick words it: "unrecognized image filter `cubicz'".
     */
    std::string_view called = {};

    constexpr std::string_view name() const { return spellings[0]; }
};

namespace Detail {

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

}

/**
 * the enumerator a name spells, in any case, or nothing: the one reader for every
 * enum a command line names -- dithers, limits, resize filters -- so what the parser
 * accepts and what the stage understands are the same list by construction.
 */
template <typename E>
constexpr std::optional<E> enumNamed(std::string_view name) {
    template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^E))) {
        if (Detail::sameName(std::meta::identifier_of(e), name)) return std::meta::extract<E>(e);
    }
    return std::nullopt;
}

/** "from 1 to 100", "one of point, box", "WxH": what the shape asks for, in words */
inline std::string shapeOf(const Option& option) {
    switch (option.shape) {
        case Shape::Any: break;
        case Shape::Integer:
            return "a whole number from " + std::to_string(option.least) + " to " + std::to_string(option.most);
        case Shape::Size: return "WxH";
        case Shape::Name: {
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
        if (option.shape == Shape::Name && !option.called.empty()) {
            return "unrecognized " + std::string(option.called) + " " + quoted(value);
        }
        return "invalid argument for option " + quoted("-" + std::string(option.name())) + ": " + std::string(value);
    };

    switch (option.shape) {
        case Shape::Any: return std::nullopt;
        case Shape::Integer: {
            const auto number = Detail::wholeNumber(value);
            if (!number || *number < option.least || *number > option.most) return why();
            return std::nullopt;
        }
        case Shape::Size: {
            const auto x = value.find_first_of("xX");
            if (x == std::string_view::npos) return why();
            const auto width = Detail::wholeNumber(value.substr(0, x));
            const auto height = Detail::wholeNumber(value.substr(x + 1));
            if (!width || !height || *width <= 0 || *height <= 0) return why();
            return std::nullopt;
        }
        case Shape::Name:
            for (const auto name : option.names) {
                if (Detail::sameName(std::string_view{name}, value)) return std::nullopt;
            }
            return why();
    }
    return std::nullopt;
}

enum class Kind {
    Input,       /**< a file, a url, stdin, or a canvas a codec draws such as xc:white */
    Transform,   /**< "-resize 8x8": between the source and the output */
    Output,      /**< the last word, and the one stage there is always exactly one of */
};

struct Stage {
    Kind kind = Kind::Transform;

    /** without the dash; the first is the canonical one */
    std::array<std::string_view, 2> spellings;

    /** its name in the grammar */
    std::string_view rule;

    /** the rule its own value has to match; empty for one that takes none, as -composite */
    std::string_view takes;

    std::span<const Option> options = {};

    std::string_view help;

    /**
     * it joins the picture from a parenthesis into the one before it, as -composite
     * does. a parenthesis is only ever closed into one of these: imagemagick would
     * keep both pictures and write two, which haio does not do.
     */
    bool merges = false;

    constexpr std::string_view name() const { return spellings[0]; }

    constexpr const Option* option(std::string_view spelled) const {
        for (const auto& option : options) {
            for (const auto spelling : option.spellings) {
                if (!spelling.empty() && spelling == spelled) return &option;
            }
        }
        return nullptr;
    }
};

/** an option a stage took, in the spelling it arrived as */
struct Taken {
    const Option* option = nullptr;
    std::string spelling;
    std::string value;
};

/** the options handed to one stage, looked up by their canonical name */
struct Given {
    std::span<const Taken> taken;

    const Taken* find(std::string_view name) const {
        for (const auto& one : taken) {
            if (one.option->spellings[0] == name) return &one;
        }
        return nullptr;
    }
};

/** why a stage would not build, and the word that is to blame */
struct Refusal {
    std::string message;
    std::string token;
};

using Built = std::expected<Token, Refusal>;

/**
 * the words of one stage, turned into what the pipeline runs.
 *
 * every stage declares its own specialisation beside its declaration; the primary is
 * deleted so that a stage somebody forgot to give one fails to compile in the parser
 * rather than at a user.
 */
template <const Stage* S>
Built build(std::string_view value, const Given& given) = delete;

}

}

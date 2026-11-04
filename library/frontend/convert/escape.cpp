#include <haio_cli.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace Haio::Cli {
namespace {

/** an escape by its long name, which is what every spelling of it comes down to */
/** an escape by its long name, shaped the way printf shapes a field: %-10t, %010B, %.3t */
struct Escape {
    std::string_view name;
    int width = 0;
    bool zeros = false;
    bool left = false;
    std::optional<int> precision;
};

/** the escapes that are numbers, the only ones a zero may pad */
constexpr std::array<std::string_view, 6> numbers{"index", "scene", "count", "width", "height", "bytes"};

/** imagemagick's single letters, each the long name it stands for */
constexpr std::array<std::pair<char, std::string_view>, 14> letters{{
    {'t', "basename"},
    {'f', "filename"},
    {'e', "extension"},
    {'i', "input"},
    {'M', "input"},
    {'w', "width"},
    {'h', "height"},
    {'p', "scene"},
    {'s', "scene"},
    {'n', "count"},
    {'m', "magick"},
    {'B', "bytes"},
    {'G', "original"},
    {'A', "alpha"},
}};

constexpr std::array<std::string_view, 8> longNames{
    "basename", "extension", "directory", "input", "width", "height", "scene", "magick",
};

/** the escapes that differ from one picture to the next, so a name holding one can name many */
constexpr std::array<std::string_view, 5> perPicture{"basename", "filename", "input", "scene", "index"};

/**
 * the name cut into what is written as it is and what is filled in, handed to visit a
 * piece at a time. %d is the scene number the way printf writes it, %03d padded with
 * zeros, as it is in an imagemagick output name; %% is a percent sign.
 */
template <typename Visit>
std::optional<Error> walk(std::string_view name, Visit&& visit) {
    const auto refused = [&](std::string_view why) {
        return Error{ErrorCode::InvalidInput, std::string(why) + " in " + Stages::quoted(std::string(name))};
    };

    std::string literal;
    for (size_t at = 0; at < name.size(); at++) {
        if (name[at] != '%') {
            literal.push_back(name[at]);
            continue;
        }
        if (++at >= name.size()) return refused("a % with nothing after it");
        if (name[at] == '%') {
            literal.push_back('%');
            continue;
        }

        visit(std::string_view{literal}, std::optional<Escape>{});
        literal.clear();

        Escape escape;
        const auto number = [&] {
            int value = 0;
            while (at < name.size() && std::isdigit(static_cast<unsigned char>(name[at]))) value = value * 10 + (name[at++] - '0');
            return value;
        };
        for (; at < name.size() && (name[at] == '-' || name[at] == '0'); at++) (name[at] == '-' ? escape.left : escape.zeros) = true;
        escape.width = number();
        if (at < name.size() && name[at] == '.') {
            at++;
            escape.precision = number();
        }
        if (at >= name.size()) return refused("a % with nothing after it");

        if (name[at] == '[') {
            const auto close = name.find(']', at);
            if (close == std::string_view::npos) return refused("a %[ that is never closed");
            const auto inside = name.substr(at + 1, close - at - 1);
            const auto known = std::ranges::find(longNames, inside);
            if (known == longNames.end()) return refused("an unknown escape %[" + std::string(inside) + "]");
            escape.name = *known;
            at = close;
        } else if (name[at] == 'd') {
            escape.name = "index";
        } else {
            const auto letter = std::ranges::find(letters, name[at], &std::pair<char, std::string_view>::first);
            if (letter == letters.end()) return refused("an unknown escape %" + std::string(1, name[at]));
            escape.name = letter->second;
        }
        if (escape.zeros && !escape.left && !std::ranges::contains(numbers, escape.name)) {
            return refused("a zero pad on %" + std::string(escape.name) + ", which is not a number");
        }
        visit(std::string_view{}, std::optional<Escape>{escape});
    }
    visit(std::string_view{literal}, std::optional<Escape>{});
    return std::nullopt;
}

}

std::string unescapeText(std::string_view text) {
    std::string out;
    for (size_t at = 0; at < text.size(); at++) {
        if (text[at] != '\\' || at + 1 == text.size()) {
            out.push_back(text[at]);
            continue;
        }
        switch (text[++at]) {
            case 'n': out.push_back('\n'); break;
            case 't': out.push_back('\t'); break;
            case 'r': out.push_back('\r'); break;
            case '\\': out.push_back('\\'); break;
            default: out.push_back('\\'); out.push_back(text[at]); break;
        }
    }
    return out;
}

bool hasEscapes(std::string_view name) {
    return name.find('%') != std::string_view::npos;
}

bool namesEach(std::string_view name) {
    bool each = false;
    const auto failed = walk(name, [&](std::string_view, const std::optional<Escape>& escape) {
        if (escape && std::ranges::contains(perPicture, escape->name)) each = true;
    });
    return !failed && each;
}

Result<std::string> expandEscapes(std::string_view name, const Written& picture, size_t index, size_t count) {
    std::string out;
    std::optional<Error> problem;

    const auto file = [&]() -> std::optional<std::filesystem::path> {
        if (picture.meta.name) return std::filesystem::path(*picture.meta.name);
        problem = Error{ErrorCode::InvalidInput, "picture " + std::to_string(index)
                                                     + " was painted rather than read, so it has no file name; use %d"};
        return std::nullopt;
    };

    const auto fill = [&](const Escape& escape) -> std::string {
        if (escape.name == "index" || escape.name == "scene") return std::to_string(index);
        if (escape.name == "count") return std::to_string(count);
        if (escape.name == "width") return std::to_string(picture.size.width);
        if (escape.name == "height") return std::to_string(picture.size.height);
        if (escape.name == "alpha") return alphaOffsetOf(picture.color) >= 0 ? "True" : "False";
        if (escape.name == "original" || escape.name == "magick" || escape.name == "bytes") {
            if (!picture.meta.magick) {
                problem = Error{ErrorCode::InvalidInput, "picture " + std::to_string(index)
                                                             + " was painted rather than read, so it has no %" + std::string(escape.name)};
                return {};
            }
            if (escape.name == "bytes") return std::to_string(picture.meta.bytes);
            if (escape.name == "original") {
                return std::to_string(picture.meta.original->width) + "x" + std::to_string(picture.meta.original->height);
            }
            auto magick = std::string(formatName(*picture.meta.magick));
            std::ranges::transform(magick, magick.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            return magick;
        }

        const auto path = file();
        if (!path) return {};
        if (escape.name == "input") return path->string();
        if (escape.name == "filename") return path->filename().string();
        if (escape.name == "basename") return path->stem().string();
        if (escape.name == "extension") {
            const auto extension = path->extension().string();
            return extension.empty() ? extension : extension.substr(1);
        }
        return path->has_parent_path() ? path->parent_path().string() : std::string(".");
    };

    const auto failed = walk(name, [&](std::string_view literal, const std::optional<Escape>& escape) {
        if (problem) return;
        out += literal;
        if (!escape) return;
        auto value = fill(*escape);
        if (escape->precision && value.size() > static_cast<size_t>(*escape->precision)) value.resize(static_cast<size_t>(*escape->precision));
        const auto pad = static_cast<size_t>(std::max(0, escape->width - static_cast<int>(value.size())));
        if (escape->left) value.append(pad, ' ');
        else value.insert(0, pad, escape->zeros ? '0' : ' ');
        out += value;
    });
    if (failed) return std::unexpected(*failed);
    if (problem) return std::unexpected(*problem);
    return out;
}

}

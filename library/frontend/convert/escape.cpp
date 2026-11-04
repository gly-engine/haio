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
struct Escape {
    std::string_view name;
    int width = 0;
    bool zeros = false;
};

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
        return Error{ErrorCode::InvalidInput, std::string(why) + " in the output name " + Stages::quoted(std::string(name))};
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

        if (name[at] == '[') {
            const auto close = name.find(']', at);
            if (close == std::string_view::npos) return refused("a %[ that is never closed");
            const auto inside = name.substr(at + 1, close - at - 1);
            const auto known = std::ranges::find(longNames, inside);
            if (known == longNames.end()) return refused("an unknown escape %[" + std::string(inside) + "]");
            visit(std::string_view{}, std::optional<Escape>{Escape{*known}});
            at = close;
            continue;
        }

        Escape escape;
        escape.zeros = name[at] == '0';
        while (at < name.size() && std::isdigit(static_cast<unsigned char>(name[at]))) {
            escape.width = escape.width * 10 + (name[at++] - '0');
        }
        if (at >= name.size()) return refused("a % with nothing after it");
        if (name[at] == 'd') {
            escape.name = "index";
        } else if (escape.width != 0 || escape.zeros) {
            return refused("a width on %" + std::string(1, name[at]) + ", which only %d takes");
        } else {
            const auto letter = std::ranges::find(letters, name[at], &std::pair<char, std::string_view>::first);
            if (letter == letters.end()) return refused("an unknown escape %" + std::string(1, name[at]));
            escape.name = letter->second;
        }
        visit(std::string_view{}, std::optional<Escape>{escape});
    }
    visit(std::string_view{literal}, std::optional<Escape>{});
    return std::nullopt;
}

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
        if (escape.name == "index" || escape.name == "scene") {
            auto digits = std::to_string(index);
            if (static_cast<int>(digits.size()) < escape.width) {
                digits.insert(0, static_cast<size_t>(escape.width) - digits.size(), escape.zeros ? '0' : ' ');
            }
            return digits;
        }
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
        if (escape) out += fill(*escape);
    });
    if (failed) return std::unexpected(*failed);
    if (problem) return std::unexpected(*problem);
    return out;
}

}

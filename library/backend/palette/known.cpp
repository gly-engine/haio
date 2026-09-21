#include <haio_palette.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <string>

namespace {

struct Known {
    std::string_view name;
    std::span<const uint32_t> colours;
};

/**
 * the master the console itself draws from, sixty four entries deep. a nes picture
 * never holds more than four at once, and which four is what a named palette below
 * is really choosing.
 */
constexpr std::array<uint32_t, 64> nes = {
    0x808080, 0x003DA6, 0x0012B0, 0x440096, 0xA1005E, 0xC70028, 0xBA0600, 0x8C1700,
    0x5C2F00, 0x104500, 0x054A00, 0x00472E, 0x004166, 0x000000, 0x050505, 0x050505,
    0xC7C7C7, 0x0077FF, 0x2155FF, 0x8237FA, 0xEB2FB5, 0xFF2950, 0xFF2200, 0xD63200,
    0xC46200, 0x358000, 0x058F00, 0x008A55, 0x0099CC, 0x212121, 0x090909, 0x090909,
    0xFFFFFF, 0x0FD7FF, 0x69A2FF, 0xD480FF, 0xFF45F3, 0xFF618B, 0xFF8833, 0xFF9C12,
    0xFABC20, 0x9FE30E, 0x2BF035, 0x0CF0A4, 0x05FBFF, 0x5E5E5E, 0x0D0D0D, 0x0D0D0D,
    0xFFFFFF, 0xA6FCFF, 0xB3ECFF, 0xDAABEB, 0xFFA8F9, 0xFFABB3, 0xFFD2B0, 0xFFEFA6,
    0xFFF79C, 0xD7E895, 0xA6EDAF, 0xA2F2DA, 0x99FFFC, 0xDDDDDD, 0x111111, 0x111111,
};

/** ibm's, the sixteen an ega card shows in text mode and everybody recognises */
constexpr std::array<uint32_t, 16> cga = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
};

constexpr std::array<uint32_t, 16> pico8 = {
    0x000000, 0x1D2B53, 0x7E2553, 0x008751, 0xAB5236, 0x5F574F, 0xC2C3C7, 0xFFF1E8,
    0xFF004D, 0xFFA300, 0xFFEC27, 0x00E436, 0x29ADFF, 0x83769C, 0xFF77A8, 0xFFCCAA,
};

constexpr std::array<uint32_t, 16> tic80 = {
    0x1A1C2C, 0x5D275D, 0xB13E53, 0xEF7D57, 0xFFCD75, 0xA7F070, 0x38B764, 0x257179,
    0x29366F, 0x3B5DC9, 0x41A6F6, 0x73EFF7, 0xF4F4F4, 0x94B0C2, 0x566C86, 0x333C57,
};

/**
 * the four colour sets chrfonts ships, written here as the colours they resolve to
 * rather than as indices into the nes master.
 *
 * keeping the resolved colour means a palette is a palette whatever console it came
 * from, and a picture converted with "mario" does not have to be a nes picture.
 */
constexpr std::array<uint32_t, 4> cgaGraphics = {0x050505, 0xFFFFFF, 0xFF45F3, 0x05FBFF};
constexpr std::array<uint32_t, 4> kokobatoru = {0x050505, 0xFFFFFF, 0xFF9C12, 0xFF2200};
constexpr std::array<uint32_t, 4> mario = {0x69A2FF, 0xFF2200, 0xFF9C12, 0xC46200};
constexpr std::array<uint32_t, 4> world = {0x69A2FF, 0x9FE30E, 0x058F00, 0x050505};

constexpr std::array<Known, 8> known = {{
    {"nes", nes},
    {"cga", cga},
    {"pico8", pico8},
    {"tic80", tic80},
    {"cgagraphics", cgaGraphics},
    {"kokobatoru", kokobatoru},
    {"mario", mario},
    {"world", world},
}};

std::optional<size_t> readNumber(std::string_view text) {
    size_t value = 0;
    const auto* end = text.data() + text.size();
    const auto parsed = std::from_chars(text.data(), end, value);
    if (parsed.ec != std::errc{} || parsed.ptr != end) return std::nullopt;
    return value;
}

}

namespace Haio {

std::span<const std::string_view> paletteNames() {
    static const auto names = [] {
        std::array<std::string_view, known.size()> out{};
        std::ranges::transform(known, out.begin(), [](const Known& one) { return one.name; });
        return out;
    }();
    return names;
}

Result<std::vector<uint32_t>> paletteNamed(std::string_view spec, size_t wanted) {
    auto name = spec;
    std::string_view slice;

    /**
     * a colon rather than brackets, because a shell eats "<" and ">" as redirections
     * and would need the whole thing quoted to reach here at all.
     */
    if (const auto colon = spec.find(':'); colon != std::string_view::npos) {
        name = spec.substr(0, colon);
        slice = spec.substr(colon + 1);
        if (slice.empty()) {
            return std::unexpected(Error{ErrorCode::InvalidInput,
                                         "\"" + std::string(spec) + "\" names nothing after its colon"});
        }
    }

    const auto found = std::ranges::find_if(known, [name](const Known& one) { return one.name == name; });
    if (found == known.end()) {
        std::string message = "no palette called \"" + std::string(name) + "\"; haio knows ";
        for (const auto& one : known) {
            message += std::string(one.name);
            if (&one != &known.back()) message += ", ";
        }
        return std::unexpected(Error{ErrorCode::InvalidInput, message});
    }

    const auto colours = found->colours;
    if (slice.empty()) return std::vector<uint32_t>(colours.begin(), colours.end());

    std::vector<uint32_t> out;

    /**
     * a bare number on its own is a bank; anything else spells out which colours.
     *
     * the two cannot be told apart by looking at the number, so they are told apart
     * by shape: "tic80:2" is the second bank of whatever the picture needs, while
     * "tic80:2..4" and "tic80:2,5" are colours, counted from one. a range is there
     * because asking for the first three of a palette should not mean writing them
     * all out.
     */
    const bool isBank = slice.find(',') == std::string_view::npos
                     && slice.find("..") == std::string_view::npos;

    if (isBank) {
        const auto bank = readNumber(slice);
        if (!bank || *bank == 0) {
            return std::unexpected(Error{ErrorCode::InvalidInput,
                                         "a palette bank is counted from one, as in " + std::string(name)
                                             + ":1, and a span is written " + std::string(name) + ":1..3"});
        }
        if (wanted == 0) {
            return std::unexpected(Error{ErrorCode::InvalidInput,
                                         "a bank is as wide as the picture needs, and this picture needs nothing"});
        }

        const auto from = (*bank - 1) * wanted;
        if (from + wanted > colours.size()) {
            return std::unexpected(Error{ErrorCode::InvalidInput,
                                         std::string(name) + " has " + std::to_string(colours.size())
                                             + " colours, which is not enough for bank " + std::to_string(*bank)
                                             + " of " + std::to_string(wanted)});
        }
        out.assign(colours.begin() + from, colours.begin() + from + wanted);
        return out;
    }

    const auto colourAt = [&](std::string_view piece) -> Result<size_t> {
        const auto at = readNumber(piece);
        if (!at || *at == 0 || *at > colours.size()) {
            return std::unexpected(Error{ErrorCode::InvalidInput,
                                         "\"" + std::string(piece) + "\" is not a colour of " + std::string(name)
                                             + ", which has " + std::to_string(colours.size())
                                             + " counted from one"});
        }
        return *at;
    };

    while (!slice.empty()) {
        const auto comma = slice.find(',');
        const auto piece = slice.substr(0, comma);

        if (const auto dots = piece.find(".."); dots != std::string_view::npos) {
            const auto first = colourAt(piece.substr(0, dots));
            if (!first) return std::unexpected(first.error());
            const auto last = colourAt(piece.substr(dots + 2));
            if (!last) return std::unexpected(last.error());

            if (*first > *last) {
                return std::unexpected(Error{ErrorCode::InvalidInput,
                                             "\"" + std::string(piece) + "\" runs backwards"});
            }
            for (auto at = *first; at <= *last; at++) out.push_back(colours[at - 1]);
        } else {
            const auto at = colourAt(piece);
            if (!at) return std::unexpected(at.error());
            out.push_back(colours[*at - 1]);
        }

        if (comma == std::string_view::npos) break;
        slice.remove_prefix(comma + 1);
    }
    return out;
}

}

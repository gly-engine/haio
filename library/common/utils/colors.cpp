#include <haio_util.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>

namespace {

struct Named {
    std::string_view name;
    uint32_t argb;
};

/**
 * imagemagick's svg colours, which are the css keywords, with imagemagick's values:
 * a canvas haio draws is the one imagemagick draws. that includes the two places it
 * parts from css -- gray is 126 and grey 190, both older than css's #808080 -- and
 * leaves out the four names only imagemagick has (fractal, freeze, matte, opaque),
 * which are not colours so much as settings.
 *
 * kept in order so a lookup is a binary search; the assert below is what keeps it so.
 */
constexpr auto named = std::to_array<Named>({
    {"aliceblue", 0xFFF0F8FFu},
    {"antiquewhite", 0xFFFAEBD7u},
    {"aqua", 0xFF00FFFFu},
    {"aquamarine", 0xFF7FFFD4u},
    {"azure", 0xFFF0FFFFu},
    {"beige", 0xFFF5F5DCu},
    {"bisque", 0xFFFFE4C4u},
    {"black", 0xFF000000u},
    {"blanchedalmond", 0xFFFFEBCDu},
    {"blue", 0xFF0000FFu},
    {"blueviolet", 0xFF8A2BE2u},
    {"brown", 0xFFA52A2Au},
    {"burlywood", 0xFFDEB887u},
    {"cadetblue", 0xFF5F9EA0u},
    {"chartreuse", 0xFF7FFF00u},
    {"chocolate", 0xFFD2691Eu},
    {"coral", 0xFFFF7F50u},
    {"cornflowerblue", 0xFF6495EDu},
    {"cornsilk", 0xFFFFF8DCu},
    {"crimson", 0xFFDC143Cu},
    {"cyan", 0xFF00FFFFu},
    {"darkblue", 0xFF00008Bu},
    {"darkcyan", 0xFF008B8Bu},
    {"darkgoldenrod", 0xFFB8860Bu},
    {"darkgray", 0xFFA9A9A9u},
    {"darkgreen", 0xFF006400u},
    {"darkgrey", 0xFFA9A9A9u},
    {"darkkhaki", 0xFFBDB76Bu},
    {"darkmagenta", 0xFF8B008Bu},
    {"darkolivegreen", 0xFF556B2Fu},
    {"darkorange", 0xFFFF8C00u},
    {"darkorchid", 0xFF9932CCu},
    {"darkred", 0xFF8B0000u},
    {"darksalmon", 0xFFE9967Au},
    {"darkseagreen", 0xFF8FBC8Fu},
    {"darkslateblue", 0xFF483D8Bu},
    {"darkslategray", 0xFF2F4F4Fu},
    {"darkslategrey", 0xFF2F4F4Fu},
    {"darkturquoise", 0xFF00CED1u},
    {"darkviolet", 0xFF9400D3u},
    {"deeppink", 0xFFFF1493u},
    {"deepskyblue", 0xFF00BFFFu},
    {"dimgray", 0xFF696969u},
    {"dimgrey", 0xFF696969u},
    {"dodgerblue", 0xFF1E90FFu},
    {"firebrick", 0xFFB22222u},
    {"floralwhite", 0xFFFFFAF0u},
    {"forestgreen", 0xFF228B22u},
    {"fuchsia", 0xFFFF00FFu},
    {"gainsboro", 0xFFDCDCDCu},
    {"ghostwhite", 0xFFF8F8FFu},
    {"gold", 0xFFFFD700u},
    {"goldenrod", 0xFFDAA520u},
    {"gray", 0xFF7E7E7Eu},
    {"green", 0xFF008000u},
    {"grey", 0xFFBEBEBEu},
    {"honeydew", 0xFFF0FFF0u},
    {"hotpink", 0xFFFF69B4u},
    {"indianred", 0xFFCD5C5Cu},
    {"indigo", 0xFF4B0082u},
    {"ivory", 0xFFFFFFF0u},
    {"khaki", 0xFFF0E68Cu},
    {"lavender", 0xFFE6E6FAu},
    {"lavenderblush", 0xFFFFF0F5u},
    {"lawngreen", 0xFF7CFC00u},
    {"lemonchiffon", 0xFFFFFACDu},
    {"lightblue", 0xFFADD8E6u},
    {"lightcoral", 0xFFF08080u},
    {"lightcyan", 0xFFE0FFFFu},
    {"lightgoldenrodyellow", 0xFFFAFAD2u},
    {"lightgray", 0xFFD3D3D3u},
    {"lightgreen", 0xFF90EE90u},
    {"lightgrey", 0xFFD3D3D3u},
    {"lightpink", 0xFFFFB6C1u},
    {"lightsalmon", 0xFFFFA07Au},
    {"lightseagreen", 0xFF20B2AAu},
    {"lightskyblue", 0xFF87CEFAu},
    {"lightslategray", 0xFF778899u},
    {"lightslategrey", 0xFF778899u},
    {"lightsteelblue", 0xFFB0C4DEu},
    {"lightyellow", 0xFFFFFFE0u},
    {"lime", 0xFF00FF00u},
    {"limegreen", 0xFF32CD32u},
    {"linen", 0xFFFAF0E6u},
    {"magenta", 0xFFFF00FFu},
    {"maroon", 0xFF800000u},
    {"mediumaquamarine", 0xFF66CDAAu},
    {"mediumblue", 0xFF0000CDu},
    {"mediumorchid", 0xFFBA55D3u},
    {"mediumpurple", 0xFF9370DBu},
    {"mediumseagreen", 0xFF3CB371u},
    {"mediumslateblue", 0xFF7B68EEu},
    {"mediumspringgreen", 0xFF00FA9Au},
    {"mediumturquoise", 0xFF48D1CCu},
    {"mediumvioletred", 0xFFC71585u},
    {"midnightblue", 0xFF191970u},
    {"mintcream", 0xFFF5FFFAu},
    {"mistyrose", 0xFFFFE4E1u},
    {"moccasin", 0xFFFFE4B5u},
    {"navajowhite", 0xFFFFDEADu},
    {"navy", 0xFF000080u},
    {"none", 0x00000000u},
    {"oldlace", 0xFFFDF5E6u},
    {"olive", 0xFF808000u},
    {"olivedrab", 0xFF6B8E23u},
    {"orange", 0xFFFFA500u},
    {"orangered", 0xFFFF4500u},
    {"orchid", 0xFFDA70D6u},
    {"palegoldenrod", 0xFFEEE8AAu},
    {"palegreen", 0xFF98FB98u},
    {"paleturquoise", 0xFFAFEEEEu},
    {"palevioletred", 0xFFDB7093u},
    {"papayawhip", 0xFFFFEFD5u},
    {"peachpuff", 0xFFFFDAB9u},
    {"peru", 0xFFCD853Fu},
    {"pink", 0xFFFFC0CBu},
    {"plum", 0xFFDDA0DDu},
    {"powderblue", 0xFFB0E0E6u},
    {"purple", 0xFF800080u},
    {"red", 0xFFFF0000u},
    {"rosybrown", 0xFFBC8F8Fu},
    {"royalblue", 0xFF4169E1u},
    {"saddlebrown", 0xFF8B4513u},
    {"salmon", 0xFFFA8072u},
    {"sandybrown", 0xFFF4A460u},
    {"seagreen", 0xFF2E8B57u},
    {"seashell", 0xFFFFF5EEu},
    {"sienna", 0xFFA0522Du},
    {"silver", 0xFFC0C0C0u},
    {"skyblue", 0xFF87CEEBu},
    {"slateblue", 0xFF6A5ACDu},
    {"slategray", 0xFF708090u},
    {"slategrey", 0xFF708090u},
    {"snow", 0xFFFFFAFAu},
    {"springgreen", 0xFF00FF7Fu},
    {"steelblue", 0xFF4682B4u},
    {"tan", 0xFFD2B48Cu},
    {"teal", 0xFF008080u},
    {"thistle", 0xFFD8BFD8u},
    {"tomato", 0xFFFF6347u},
    {"transparent", 0x00000000u},
    {"turquoise", 0xFF40E0D0u},
    {"violet", 0xFFEE82EEu},
    {"wheat", 0xFFF5DEB3u},
    {"white", 0xFFFFFFFFu},
    {"whitesmoke", 0xFFF5F5F5u},
    {"yellow", 0xFFFFFF00u},
    {"yellowgreen", 0xFF9ACD32u},
});

static_assert(std::ranges::is_sorted(named, {}, &Named::name), "the colour table has to stay in alphabetical order");
static_assert(std::ranges::adjacent_find(named, {}, &Named::name) == named.end(), "a colour is named twice");

std::optional<uint32_t> hexDigit(char c) {
    if (c >= '0' && c <= '9') return static_cast<uint32_t>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<uint32_t>(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return static_cast<uint32_t>(c - 'A' + 10);
    return std::nullopt;
}

/** #rgb, #rgba, #rrggbb or #rrggbbaa, alpha last the way css writes it */
std::optional<uint32_t> hexColour(std::string_view text) {
    if (!text.starts_with('#')) return std::nullopt;
    text.remove_prefix(1);

    const bool shortForm = text.size() == 3 || text.size() == 4;
    if (!shortForm && text.size() != 6 && text.size() != 8) return std::nullopt;

    const size_t width = shortForm ? 1 : 2;
    uint32_t channels[4] = {0, 0, 0, 255};
    for (size_t at = 0; at * width < text.size(); at++) {
        uint32_t value = 0;
        for (size_t i = 0; i < width; i++) {
            const auto digit = hexDigit(text[at * width + i]);
            if (!digit) return std::nullopt;
            value = value * 16 + *digit;
        }
        channels[at] = shortForm ? value * 17 : value;
    }
    return (channels[3] << 24) | (channels[0] << 16) | (channels[1] << 8) | channels[2];
}

/**
 * gray0 to gray100, and grey the same, which x11 and imagemagick spell as a share of
 * white rather than keeping two hundred rows for it. the rounding is x11's, and gives
 * back every one of imagemagick's values.
 */
std::optional<uint32_t> greyShare(std::string_view key) {
    if (!key.starts_with("gray") && !key.starts_with("grey")) return std::nullopt;
    const auto digits = key.substr(4);
    if (digits.empty()) return std::nullopt;

    int share = 0;
    const auto [end, problem] = std::from_chars(digits.data(), digits.data() + digits.size(), share);
    if (problem != std::errc{} || end != digits.data() + digits.size() || share < 0 || share > 100) return std::nullopt;

    const auto level = static_cast<uint32_t>(share * 2.55 + 0.5);
    return 0xFF000000u | (level << 16) | (level << 8) | level;
}

}

namespace Haio::Util {

std::optional<uint32_t> GetColorFromName(std::string_view name) {
    if (const auto hex = hexColour(name)) return hex;

    // imagemagick reads AliceBlue and aliceblue alike, and so does css
    std::string key(name);
    std::ranges::transform(key, key.begin(), [](unsigned char c) {
        return static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    });

    const auto found = std::ranges::lower_bound(named, std::string_view{key}, {}, &Named::name);
    if (found != named.end() && found->name == key) return found->argb;
    return greyShare(key);
}

}

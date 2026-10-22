#include <haio/codecs/generators/text.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SYNTHESIS_H

#include <haio/generated/fonts/noto_sans.h>

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Haio::Codecs {
namespace {

struct Style {
    bool bold = false;
    bool italic = false;
};

/** the words bold, italic and regular, in any order, case, spacing or dashes */
std::optional<Style> styleOf(std::string_view text) {
    std::string squashed;
    for (const char c : text) {
        if (c == ' ' || c == '-' || c == '_') continue;
        squashed += (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    }
    Style out;
    for (std::string_view rest = squashed; !rest.empty();) {
        if (rest.starts_with("bold")) out.bold = true, rest.remove_prefix(4);
        else if (rest.starts_with("italic")) out.italic = true, rest.remove_prefix(6);
        else if (rest.starts_with("oblique")) out.italic = true, rest.remove_prefix(7);
        else if (rest.starts_with("regular")) rest.remove_prefix(7);
        else return std::nullopt;
    }
    return out;
}

using Library = std::unique_ptr<std::remove_pointer_t<FT_Library>, decltype(&FT_Done_FreeType)>;
using Face = std::unique_ptr<std::remove_pointer_t<FT_Face>, decltype(&FT_Done_Face)>;

Face open(FT_Library library, const std::string& path, FT_Long index) {
    FT_Face face = nullptr;
    if (FT_New_Face(library, path.c_str(), index, &face)) face = nullptr;
    return Face{face, FT_Done_Face};
}

/** the noto sans built into the binary */
Face builtIn(FT_Library library) {
    FT_Face face = nullptr;
    if (FT_New_Memory_Face(library, NotoSans_Regular, static_cast<FT_Long>(NotoSans_Regular_len), 0, &face)) face = nullptr;
    return Face{face, FT_Done_Face};
}

std::vector<std::filesystem::path> fontDirectories() {
    std::vector<std::filesystem::path> out{"/usr/share/fonts", "/usr/local/share/fonts"};
    if (const char* home = std::getenv("HOME")) {
        out.emplace_back(std::filesystem::path(home) / ".local/share/fonts");
        out.emplace_back(std::filesystem::path(home) / ".fonts");
    }
    return out;
}

bool isFontFile(const std::filesystem::path& path) {
    auto extension = path.extension().string();
    for (auto& c : extension) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return extension == ".ttf" || extension == ".otf" || extension == ".ttc" || extension == ".otc";
}

/**
 * the face of that family closest to the style: the one with both flags right, and of
 * those the plainest name, so "Bold" wins over "Condensed Bold". failing that, the one
 * with none of the flags wrong, for the missing ones to be made up.
 */
Face find(FT_Library library, std::string_view family, Style style) {
    Face best{nullptr, FT_Done_Face};
    int bestScore = INT_MAX;
    for (const auto& directory : fontDirectories()) {
        std::error_code problem;
        for (std::filesystem::recursive_directory_iterator it(directory, problem), end; !problem && it != end; it.increment(problem)) {
            if (!it->is_regular_file(problem) || !isFontFile(it->path())) continue;
            const auto path = it->path().string();
            auto first = open(library, path, 0);
            const FT_Long faces = first ? first->num_faces : 0;
            for (FT_Long index = 0; index < faces; index++) {
                auto face = index == 0 ? std::move(first) : open(library, path, index);
                if (!face || !face->family_name || !Stages::Detail::sameName(face->family_name, family)) continue;
                const bool bold = face->style_flags & FT_STYLE_FLAG_BOLD;
                const bool italic = face->style_flags & FT_STYLE_FLAG_ITALIC;
                if ((bold && !style.bold) || (italic && !style.italic)) continue;
                const int missing = (style.bold != bold) + (style.italic != italic);
                const int score = missing * 1000 + static_cast<int>(std::string_view{face->style_name ? face->style_name : ""}.size());
                if (score < bestScore) bestScore = score, best = std::move(face);
            }
        }
    }
    return best;
}

std::vector<char32_t> codepointsOf(std::string_view text) {
    std::vector<char32_t> out;
    for (size_t i = 0; i < text.size();) {
        const auto byte = static_cast<unsigned char>(text[i]);
        const int length = byte < 0x80 ? 1 : (byte >> 5) == 0x6 ? 2 : (byte >> 4) == 0xE ? 3 : (byte >> 3) == 0x1E ? 4 : 0;
        if (length == 0 || i + length > text.size()) {
            out.push_back(U'�');
            i++;
            continue;
        }
        char32_t point = length == 1 ? byte : byte & (0x7F >> length);
        for (int k = 1; k < length; k++) point = (point << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3F);
        out.push_back(point);
        i += length;
    }
    return out;
}

/** a glyph's coverage, and where its top left corner goes relative to the first baseline */
struct Glyph {
    int left = 0, top = 0, width = 0, height = 0;
    std::vector<uint8_t> coverage;
};

}

/**
 * words in a font, black on opaque white. a line break, or a \n written out, starts a
 * new line. without -size the picture is the lines' box, grown where a glyph reaches
 * past it, as an italic does; with one the text is centred in it.
 */
template <>
Result<Image<Color::GRAY8>> Generate<Brush::Text, Color::GRAY8>(std::string_view words, const Settings& settings) {
    if (words.empty()) HAIO_FAIL(InvalidInput, "nothing to write; it goes after the colon, as in text:hello");

    HAIO_TRY(pixels, settingInt(settings, fontSize));
    const auto* styled = settingNamed(settings, fontStyle.name());
    const auto style = styleOf(styled ? std::string_view{styled->value} : fontStyle.fallback);
    if (!style) HAIO_FAIL(InvalidInput, "unrecognized font style " + Stages::quoted(styled->value));

    FT_Library raw = nullptr;
    if (FT_Init_FreeType(&raw)) HAIO_FAIL(Internal, "freetype could not start");
    const Library library{raw, FT_Done_FreeType};

    const auto* named = settingNamed(settings, fontName.name());
    const std::string font{named ? std::string_view{named->value} : fontName.fallback};
    const bool path = font.find('/') != std::string::npos || isFontFile(font);
    // nobody named one, so the built in one, the same picture on every machine
    auto face = !named ? builtIn(library.get()) : path ? open(library.get(), font, 0) : find(library.get(), font, *style);
    if (!face && !path && Stages::Detail::sameName(font, fontName.fallback)) face = builtIn(library.get());
    if (!face) {
        if (path) HAIO_FAIL(InvalidInput, "unable to read font " + Stages::quoted(font));
        HAIO_FAIL(NotFound, "font " + Stages::quoted(font) + " not found in the system's font directories; a path to the file will do");
    }
    if (FT_Set_Pixel_Sizes(face.get(), 0, static_cast<FT_UInt>(pixels))) {
        HAIO_FAIL(InvalidInput, "font " + Stages::quoted(font) + " has no size " + std::to_string(pixels));
    }
    const bool embolden = style->bold && !(face->style_flags & FT_STYLE_FLAG_BOLD);
    const bool slant = style->italic && !(face->style_flags & FT_STYLE_FLAG_ITALIC);

    const auto& metrics = face->size->metrics;
    const int ascender = static_cast<int>((metrics.ascender + 63) >> 6);
    const int descender = static_cast<int>(-metrics.descender >> 6);
    const int lineHeight = static_cast<int>((metrics.height + 63) >> 6);

    std::vector<Glyph> glyphs;
    int lines = 1;
    int widest = 0;
    int minX = 0, minY = -ascender, maxX = 0, maxY = descender;
    FT_Pos pen = 0;
    FT_UInt previous = 0;
    const auto points = codepointsOf(words);
    for (size_t i = 0; i < points.size(); i++) {
        const bool escaped = points[i] == U'\\' && i + 1 < points.size() && points[i + 1] == U'n';
        if (points[i] == U'\n' || escaped) {
            if (escaped) i++;
            lines++;
            pen = 0;
            previous = 0;
            maxY += lineHeight;
            continue;
        }
        const FT_UInt index = FT_Get_Char_Index(face.get(), points[i]);
        if (previous && index && FT_HAS_KERNING(face.get())) {
            FT_Vector kerning;
            if (!FT_Get_Kerning(face.get(), previous, index, FT_KERNING_DEFAULT, &kerning)) pen += kerning.x;
        }
        previous = index;
        if (FT_Load_Glyph(face.get(), index, FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP)) continue;
        auto* slot = face->glyph;
        if (embolden) FT_GlyphSlot_Embolden(slot);
        if (slant) FT_GlyphSlot_Oblique(slot);
        if (FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL)) continue;

        const auto& bitmap = slot->bitmap;
        Glyph glyph{static_cast<int>(pen >> 6) + slot->bitmap_left, (lines - 1) * lineHeight - slot->bitmap_top,
                    static_cast<int>(bitmap.width), static_cast<int>(bitmap.rows), {}};
        glyph.coverage.resize(static_cast<size_t>(glyph.width) * static_cast<size_t>(glyph.height));
        for (int y = 0; y < glyph.height; y++) {
            std::copy_n(bitmap.buffer + static_cast<ptrdiff_t>(y) * bitmap.pitch, glyph.width,
                        glyph.coverage.begin() + static_cast<ptrdiff_t>(y) * glyph.width);
        }
        if (glyph.width && glyph.height) {
            minX = std::min(minX, glyph.left);
            minY = std::min(minY, glyph.top);
            maxX = std::max(maxX, glyph.left + glyph.width);
            maxY = std::max(maxY, glyph.top + glyph.height);
            glyphs.push_back(std::move(glyph));
        }
        pen += slot->advance.x;
        widest = std::max(widest, static_cast<int>((pen + 63) >> 6));
    }
    maxX = std::max(maxX, widest);

    const int inkWidth = std::max(1, maxX - minX);
    const int inkHeight = std::max(1, maxY - minY);
    if (inkWidth > Canvas::largest || inkHeight > Canvas::largest) {
        HAIO_FAIL(InvalidInput, "text of " + std::to_string(inkWidth) + "x" + std::to_string(inkHeight)
                                    + " exceeds limit of " + std::to_string(Canvas::largest));
    }

    Size size{inkWidth, inkHeight};
    if (settingNamed(settings, textSize.name())) {
        HAIO_TRY(wanted, Canvas::sizeOf(settings));
        size = wanted;
    }
    const int offsetX = (size.width - inkWidth) / 2 - minX;
    const int offsetY = (size.height - inkHeight) / 2 - minY;

    Image<Color::GRAY8> image{size.width, size.height,
                              std::vector<uint8_t>(static_cast<size_t>(size.width) * static_cast<size_t>(size.height), 0xFF)};
    for (const auto& glyph : glyphs) {
        for (int y = 0; y < glyph.height; y++) {
            const int row = offsetY + glyph.top + y;
            if (row < 0 || row >= size.height) continue;
            for (int x = 0; x < glyph.width; x++) {
                const int column = offsetX + glyph.left + x;
                if (column < 0 || column >= size.width) continue;
                const int ink = glyph.coverage[static_cast<size_t>(y) * static_cast<size_t>(glyph.width) + static_cast<size_t>(x)];
                auto& pixel = image.data[static_cast<size_t>(row) * static_cast<size_t>(size.width) + static_cast<size_t>(column)];
                pixel = static_cast<uint8_t>((pixel * (255 - ink) + 127) / 255);
            }
        }
    }
    return image;
}

}

#include <haio/implementation/registry.hpp>
#include <haio/implementation/language/query.hpp>
#include <haio/util/string.hpp>
#include <haio/util/url.hpp>

#include <stdexcept>

namespace Haio::Implementation::LanguageParser {

std::unordered_map<std::string, std::string> parseQueryMap(std::string_view query) {
    std::unordered_map<std::string, std::string> out;
    if (!query.empty() && query.front() == '?') query.remove_prefix(1);
    if (query.empty()) return out;

    const auto parsed = Util::Url::parse("/?" + std::string(query));
    if (!parsed) throw std::runtime_error("invalid query: " + std::string(query));

    for (auto& [key, value] : parsed->params(true)) {
        if (!key.empty()) out[key] = std::move(value);
    }
    return out;
}

std::vector<Language::Token> parseQueryTokens(std::string_view query) {
    const auto values = parseQueryMap(query);
    std::vector<Language::Token> tokens;

    if (auto it = values.find("crop"); it != values.end()) tokens.push_back(LanguageToken::Crop(Util::getRect(it->second)));
    /**
     * a url cannot carry a percent sign: "%" opens an escape, so "?resize=30%" is a
     * broken request rather than a small picture. "30pct" says the same thing and
     * survives, and both spellings are read here so a query and a command line can
     * be written the same way.
     */
    const auto resizeToken = [](const std::string& value) {
        const auto share = Util::getPercent(value);
        return share != 0 ? LanguageToken::ResizeByPercent(share) : LanguageToken::Resize(Util::getSize(value));
    };

    if (auto it = values.find("size"); it != values.end()) tokens.push_back(resizeToken(it->second));
    if (auto it = values.find("resize"); it != values.end()) tokens.push_back(resizeToken(it->second));
    if (auto it = values.find("radius"); it != values.end()) tokens.push_back(LanguageToken::Radius(Util::getInt(it->second)));
    /**
     * the colour inside the container, spelled the way ffmpeg spells it.
     *
     * it can arrive without a format, and that is a request rather than a mistake:
     * "?pix_fmt=rgb565" with nothing to wrap it is raw pixels in that colour, which
     * is what a caller loading a texture straight into a gpu is asking for.
     */
    auto pixelFormat = values.find("pix_fmt");
    if (pixelFormat == values.end()) pixelFormat = values.find("pix_format");

    std::optional<Color> color;
    if (pixelFormat != values.end()) {
        color = colorNamed(pixelFormat->second);
        if (!color) throw std::runtime_error("unknown pixel format: " + pixelFormat->second);
    }

    if (auto it = values.find("format"); it != values.end()) {
        tokens.push_back(LanguageToken::Encode(formatFromName(it->second), color));
    } else if (color) {
        tokens.push_back(LanguageToken::Encode(Format::RAW, color));
    }

    return tokens;
}

}

#include <haio.hpp>
#include <haio_codecs.hpp>

#include <algorithm>
#include <cctype>

namespace {

std::string lower(std::string_view value) {
    std::string out(value);
    std::ranges::transform(out, out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

std::string_view lastExtension(std::string_view path) {
    const auto slash = path.find_last_of("/\\");
    const auto dot = path.find_last_of('.');
    if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash)) return {};
    return path.substr(dot + 1);
}

}

namespace Haio {

/**
 * a file name only ever names a container, never the colour inside it. the extension
 * is not the enumerator name: .jpg is the usual spelling of jpeg, and the netpbm
 * family answers to four.
 */
Format formatFromExtension(std::string_view path) {
    const auto key = lower(lastExtension(path));
    for (const auto& entry : formatExtensions) {
        if (entry.extension == key) return entry.format;
    }
    return Format::RAW;
}

Format formatFromContentType(std::string_view contentType) {
    auto value = contentType.substr(0, contentType.find(';'));
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.remove_prefix(1);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.remove_suffix(1);

    const auto key = lower(value);
    for (const auto& entry : formatMimes) {
        if (entry.mime == key) return entry.format;
    }
    return Format::RAW;
}

/** the first extension an enumerator names is the one haio writes */
std::string_view extensionFor(Format format) {
    for (const auto& entry : formatExtensions) {
        if (entry.format == format) return entry.extension;
    }
    return "bin";
}

/** the first mime type an enumerator names is the one haio sends */
std::string_view contentTypeFor(Format format) {
    for (const auto& entry : formatMimes) {
        if (entry.format == format) return entry.mime;
    }
    return "application/octet-stream";
}
}

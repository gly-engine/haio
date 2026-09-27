#include <haio/internal/source/archive.hpp>

#include <string>
#include <string_view>

namespace {

constexpr std::string_view magic = "!<arch>\n";
constexpr size_t headerSize = 60;

std::string_view trimmed(std::string_view text) {
    while (!text.empty() && (text.back() == ' ' || text.back() == '\0')) text.remove_suffix(1);
    return text;
}

std::optional<uint64_t> decimal(std::string_view text) {
    text = trimmed(text);
    if (text.empty()) return std::nullopt;
    uint64_t value = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') return std::nullopt;
        value = value * 10 + static_cast<uint64_t>(c - '0');
    }
    return value;
}

}

namespace Haio::Source {

/**
 * an ar is a magic line and then headers of 60 bytes, each followed by its file padded
 * to an even length. it is what an ipk and a deb are made of: debian-binary, then
 * control.tar.gz, then data.tar.gz.
 *
 * names are sixteen bytes, so the longer ones live elsewhere. gnu keeps them all in a
 * member called "//" and names the file "/<offset>" into it; bsd writes "#1/<length>"
 * and puts the name at the front of the file itself.
 */
Result<ArchiveIndex> readArIndex(Bytes data) {
    const auto text = std::string_view(reinterpret_cast<const char*>(data.data()), data.size());
    if (!text.starts_with(magic)) HAIO_FAIL(InvalidInput, "not an ar archive");

    ArchiveIndex index;
    std::string_view longNames;
    size_t at = magic.size();

    while (at + headerSize <= text.size()) {
        const auto header = text.substr(at, headerSize);
        if (header.substr(58, 2) != "`\n") HAIO_FAIL(InvalidInput, "an ar header is damaged");

        const auto size = decimal(header.substr(48, 10));
        if (!size) HAIO_FAIL(InvalidInput, "an ar header has a size that is not a number");

        auto from = at + headerSize;
        if (*size > text.size() - from) HAIO_FAIL(InvalidInput, "an ar member runs past the end of the file");
        const auto next = from + *size + (*size & 1);

        auto length = *size;
        auto rawName = trimmed(header.substr(0, 16));
        std::string name;

        if (rawName == "/" || rawName == "/SYM64/" || rawName == "__.SYMDEF" || rawName == "__.SYMDEF SORTED") {
            // the symbol table of a static library, which names no file
        } else if (rawName == "//") {
            longNames = text.substr(from, length);
        } else if (rawName.starts_with("#1/")) {
            const auto nameSize = decimal(rawName.substr(3));
            if (!nameSize || *nameSize > length) HAIO_FAIL(InvalidInput, "an ar member name runs past the member");
            name = std::string(trimmed(text.substr(from, *nameSize)));
            from += *nameSize;
            length -= *nameSize;
        } else if (rawName.starts_with('/') && rawName.size() > 1) {
            const auto offset = decimal(rawName.substr(1));
            if (!offset || *offset >= longNames.size()) HAIO_FAIL(InvalidInput, "an ar member names a long name that is not there");
            auto found = longNames.substr(*offset);
            found = found.substr(0, found.find('\n'));
            if (found.ends_with('/')) found.remove_suffix(1);
            name = std::string(found);
        } else {
            // gnu ends a short name with a slash so that one may contain spaces
            if (rawName.ends_with('/')) rawName.remove_suffix(1);
            name = std::string(rawName);
        }

        if (!name.empty() && name.find('/') == std::string::npos && name != "..") {
            index.entries[std::move(name)] = ArchiveEntry{from, length, length};
        }
        at = next;
    }

    // whatever is left is less than a header, which is a member cut short
    if (at < text.size()) HAIO_FAIL(InvalidInput, "an ar archive is cut short");
    return index;
}

}

#include <haio/internal/source/archive.hpp>

#include <string>
#include <string_view>

namespace {

using Haio::Bytes;

constexpr size_t block = 512;

/** a header field is nul terminated when it is shorter than the field, and not when it fills it */
std::string_view field(Bytes header, size_t at, size_t size) {
    const auto* begin = reinterpret_cast<const char*>(header.data()) + at;
    std::string_view text(begin, size);
    return text.substr(0, text.find('\0'));
}

/**
 * sizes are octal text, padded with spaces or nuls. a file over 8gb does not fit in
 * eleven octal digits, and gnu tar then writes the size in binary with the top bit set.
 */
std::optional<uint64_t> number(Bytes header, size_t at, size_t size) {
    if (header[at] & 0x80) {
        uint64_t value = 0;
        for (size_t i = at + 1; i < at + size; i++) value = (value << 8) | header[i];
        return value;
    }

    uint64_t value = 0;
    bool any = false;
    for (size_t i = at; i < at + size; i++) {
        const char c = static_cast<char>(header[i]);
        if (c == ' ' || c == '\0') {
            if (any) break;
            continue;
        }
        if (c < '0' || c > '7') return std::nullopt;
        value = value * 8 + static_cast<uint64_t>(c - '0');
        any = true;
    }
    return value;
}

/**
 * the checksum is the sum of the header's bytes with its own field read as spaces. it
 * is what tells a tar from anything else, since the oldest ones carry no magic.
 */
bool checksumHolds(Bytes header) {
    const auto stored = number(header, 148, 8);
    if (!stored) return false;

    uint64_t sum = 0;
    for (size_t i = 0; i < block; i++) sum += (i >= 148 && i < 156) ? ' ' : header[i];
    return sum == *stored;
}

bool allZero(Bytes header) {
    for (const auto byte : header) {
        if (byte != 0) return false;
    }
    return true;
}

/** "./assets/icon.png" and "/assets/icon.png" are both "assets/icon.png" */
std::string normalised(std::string_view name) {
    while (true) {
        if (name.starts_with("./")) name.remove_prefix(2);
        else if (name.starts_with('/')) name.remove_prefix(1);
        else break;
    }
    return std::string(name);
}

bool climbs(std::string_view name) {
    return name == ".." || name.starts_with("../") || name.ends_with("/..") || name.find("/../") != std::string_view::npos;
}

/** a pax header is records of "<length> <key>=<value>\n"; only the path is wanted */
std::string paxPath(std::string_view records) {
    while (!records.empty()) {
        const auto space = records.find(' ');
        if (space == std::string_view::npos) break;

        size_t length = 0;
        for (const char c : records.substr(0, space)) {
            if (c < '0' || c > '9') return {};
            length = length * 10 + static_cast<size_t>(c - '0');
        }
        if (length <= space || length > records.size()) break;

        const auto record = records.substr(space + 1, length - space - 2);
        if (record.starts_with("path=")) return std::string(record.substr(5));
        records.remove_prefix(length);
    }
    return {};
}

}

namespace Haio::Source {

/**
 * a tar is headers of 512 bytes, each followed by its file padded to 512, and two
 * blocks of zeroes at the end. names longer than a header holds arrive in an entry of
 * their own just before the file, as gnu's "L" or as a pax "x" with a path in it.
 */
Result<ArchiveIndex> readTarIndex(Bytes data) {
    ArchiveIndex index;
    std::string longName;
    size_t at = 0;

    while (at + block <= data.size()) {
        const auto header = data.subspan(at, block);
        if (allZero(header)) break;
        if (!checksumHolds(header)) HAIO_FAIL(InvalidInput, "not a tar, or a tar header is damaged");

        const auto size = number(header, 124, 12);
        if (!size) HAIO_FAIL(InvalidInput, "a tar header has a size that is not a number");

        const auto from = at + block;
        if (*size > data.size() - from) HAIO_FAIL(InvalidInput, "a tar entry runs past the end of the file");
        const auto body = std::string_view(reinterpret_cast<const char*>(data.data()) + from, *size);

        const char type = static_cast<char>(header[156]);
        if (type == 'L') {
            longName = std::string(body.substr(0, body.find('\0')));
        } else if (type == 'x') {
            longName = paxPath(body);
        } else if (type == '0' || type == '\0' || type == '7') {
            auto name = longName;
            if (name.empty()) {
                name = std::string(field(header, 0, 100));
                // ustar splits a long name in two, the front half in the prefix field
                if (field(header, 257, 5) == "ustar") {
                    const auto prefix = field(header, 345, 155);
                    if (!prefix.empty()) name = std::string(prefix) + "/" + name;
                }
            }
            name = normalised(name);
            // a directory has nothing to read, and a name that climbs is not read at all
            if (!name.empty() && name.back() != '/' && !climbs(name)) {
                index.entries[std::move(name)] = ArchiveEntry{from, *size, *size};
            }
            longName.clear();
        } else {
            // directories, links, devices and pax globals carry nothing to read
            longName.clear();
        }

        at = from + (*size + block - 1) / block * block;
    }

    if (at == 0) HAIO_FAIL(InvalidInput, "not a tar");
    return index;
}

}

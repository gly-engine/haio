#include <haio/internal/source/archive.hpp>

#include <zlib.h>

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace Haio;
using namespace Haio::Source;

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (ok) return;
    std::cerr << "fail: " << what << '\n';
    failures++;
}

void put16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>(value));
    out.push_back(static_cast<uint8_t>(value >> 8));
}

void put32(std::vector<uint8_t>& out, uint32_t value) {
    for (int at = 0; at < 4; at++) out.push_back(static_cast<uint8_t>(value >> (at * 8)));
}

std::vector<uint8_t> deflateRaw(const std::vector<uint8_t>& raw) {
    std::vector<uint8_t> out(compressBound(static_cast<uLong>(raw.size())) + 64);

    z_stream stream{};
    deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    stream.next_in = const_cast<Bytef*>(raw.data());
    stream.avail_in = static_cast<uInt>(raw.size());
    stream.next_out = out.data();
    stream.avail_out = static_cast<uInt>(out.size());
    deflate(&stream, Z_FINISH);
    out.resize(stream.total_out);
    deflateEnd(&stream);
    return out;
}

struct Adding {
    std::string name;
    std::vector<uint8_t> raw;
    bool compress = true;
};

/**
 * the test writes the archive it then reads, so the reader is checked against offsets
 * built here rather than against a blob nobody can inspect.
 */
std::vector<uint8_t> makeZip(const std::vector<Adding>& adding) {
    std::vector<uint8_t> zip;
    struct Placed { std::string name; uint32_t at; uint32_t packed; uint32_t plain; uint16_t method; };
    std::vector<Placed> placed;

    for (const auto& entry : adding) {
        const auto at = static_cast<uint32_t>(zip.size());
        const auto body = entry.compress ? deflateRaw(entry.raw) : entry.raw;
        const uint16_t method = entry.compress ? 8 : 0;

        put32(zip, 0x04034b50);
        put16(zip, 20); put16(zip, 0); put16(zip, method);
        put16(zip, 0); put16(zip, 0);
        put32(zip, 0);
        put32(zip, static_cast<uint32_t>(body.size()));
        put32(zip, static_cast<uint32_t>(entry.raw.size()));
        put16(zip, static_cast<uint16_t>(entry.name.size())); put16(zip, 0);
        zip.insert(zip.end(), entry.name.begin(), entry.name.end());
        zip.insert(zip.end(), body.begin(), body.end());

        placed.push_back(Placed{entry.name, at, static_cast<uint32_t>(body.size()),
                                static_cast<uint32_t>(entry.raw.size()), method});
    }

    const auto directoryAt = static_cast<uint32_t>(zip.size());
    for (const auto& entry : placed) {
        put32(zip, 0x02014b50);
        put16(zip, 20); put16(zip, 20); put16(zip, 0); put16(zip, entry.method);
        put16(zip, 0); put16(zip, 0);
        put32(zip, 0);
        put32(zip, entry.packed);
        put32(zip, entry.plain);
        put16(zip, static_cast<uint16_t>(entry.name.size()));
        put16(zip, 0); put16(zip, 0); put16(zip, 0); put16(zip, 0);
        put32(zip, 0);
        put32(zip, entry.at);
        zip.insert(zip.end(), entry.name.begin(), entry.name.end());
    }
    const auto directorySize = static_cast<uint32_t>(zip.size()) - directoryAt;

    put32(zip, 0x06054b50);
    put16(zip, 0); put16(zip, 0);
    put16(zip, static_cast<uint16_t>(placed.size()));
    put16(zip, static_cast<uint16_t>(placed.size()));
    put32(zip, directorySize);
    put32(zip, directoryAt);
    put16(zip, 0);
    return zip;
}

std::vector<uint8_t> bytesOf(std::string_view text) { return {text.begin(), text.end()}; }
std::string textOf(const std::vector<uint8_t>& raw) { return {raw.begin(), raw.end()}; }

constexpr size_t plenty = 64u << 20;

std::vector<uint8_t> gzipOf(const std::vector<uint8_t>& raw) {
    std::vector<uint8_t> out(compressBound(static_cast<uLong>(raw.size())) + 64);
    z_stream stream{};
    deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    stream.next_in = const_cast<Bytef*>(raw.data());
    stream.avail_in = static_cast<uInt>(raw.size());
    stream.next_out = out.data();
    stream.avail_out = static_cast<uInt>(out.size());
    deflate(&stream, Z_FINISH);
    out.resize(stream.total_out);
    deflateEnd(&stream);
    return out;
}

void octal(std::vector<uint8_t>& header, size_t at, size_t width, uint64_t value) {
    for (size_t i = width - 1; i-- > 0;) {
        header[at + i] = static_cast<uint8_t>('0' + (value & 7));
        value >>= 3;
    }
    header[at + width - 1] = 0;
}

std::vector<uint8_t> tarHeader(std::string_view name, char type, size_t size, std::string_view prefix = {}) {
    std::vector<uint8_t> header(512, 0);
    std::copy(name.begin(), name.end(), header.begin());
    octal(header, 100, 8, 0644);
    octal(header, 108, 8, 0);
    octal(header, 116, 8, 0);
    octal(header, 124, 12, size);
    octal(header, 136, 12, 0);
    header[156] = static_cast<uint8_t>(type);
    std::copy_n("ustar", 6, header.begin() + 257);
    header[263] = '0';
    header[264] = '0';
    std::copy(prefix.begin(), prefix.end(), header.begin() + 345);

    std::fill(header.begin() + 148, header.begin() + 156, ' ');
    unsigned sum = 0;
    for (const auto byte : header) sum += byte;
    octal(header, 148, 7, sum);
    header[155] = ' ';
    return header;
}

void padded(std::vector<uint8_t>& out, const std::vector<uint8_t>& body) {
    out.insert(out.end(), body.begin(), body.end());
    out.resize((out.size() + 511) / 512 * 512, 0);
}

enum class LongName { Gnu, Pax };

/** a tar written the way gnu tar and busybox write them, long names included */
std::vector<uint8_t> makeTar(const std::vector<std::pair<std::string, std::vector<uint8_t>>> files, LongName longNames = LongName::Gnu) {
    std::vector<uint8_t> tar;
    for (const auto& [name, body] : files) {
        if (name.size() >= 100) {
            if (longNames == LongName::Gnu) {
                const auto header = tarHeader("././@LongLink", 'L', name.size() + 1);
                tar.insert(tar.end(), header.begin(), header.end());
                auto text = bytesOf(name);
                text.push_back(0);
                padded(tar, text);
            } else {
                auto record = " path=" + name + "\n";
                auto length = record.size() + 2;
                if (std::to_string(length).size() != 2) length++;
                record = std::to_string(length) + record;
                const auto header = tarHeader("PaxHeader", 'x', record.size());
                tar.insert(tar.end(), header.begin(), header.end());
                padded(tar, bytesOf(record));
            }
        }
        const auto header = tarHeader(name.substr(0, 99), '0', body.size());
        tar.insert(tar.end(), header.begin(), header.end());
        padded(tar, body);
    }
    tar.resize(tar.size() + 1024, 0);
    return tar;
}

/** an ar as opkg writes an ipk: gnu names ending in a slash, members padded to even */
std::vector<uint8_t> makeAr(const std::vector<std::pair<std::string, std::vector<uint8_t>>> members, bool bsdNames = false) {
    std::vector<uint8_t> ar = bytesOf("!<arch>\n");
    for (const auto& [name, body] : members) {
        auto field = [&](std::string text, size_t width) {
            text.resize(width, ' ');
            ar.insert(ar.end(), text.begin(), text.end());
        };
        const auto size = bsdNames ? body.size() + name.size() : body.size();
        field(bsdNames ? "#1/" + std::to_string(name.size()) : name + "/", 16);
        field("0", 12);
        field("0", 6);
        field("0", 6);
        field("100644", 8);
        field(std::to_string(size), 10);
        field("`\n", 2);
        if (bsdNames) ar.insert(ar.end(), name.begin(), name.end());
        ar.insert(ar.end(), body.begin(), body.end());
        if (size & 1) ar.push_back('\n');
    }
    return ar;
}

Result<Blob> dig(const std::vector<uint8_t>& archive, std::string_view name, std::string_view inside, Archives* archives = nullptr) {
    return readInsideArchive(archive, std::string(name), inside, ArchiveOptions{plenty, archives});
}

}

auto main() -> int {
    const auto picture = bytesOf(std::string(4096, 'P'));
    const auto note = bytesOf("not an image");

    const auto zip = makeZip({{"logo.png", picture, true},
                              {"nested/pic.ppm", note, false},
                              {"notes.txt", note, true}});

    // zip: the index, deflate and stored entries, and the limits
    {
        auto index = readZipIndex(zip);
        check(index.has_value(), "a zip this test wrote reads back");
        if (!index) return 1;
        check(index->entries.size() == 3, "every entry is indexed");
        check(index->entries.contains("nested/pic.ppm"), "a name with a directory in it is kept whole");

        const auto deflated = index->entries.at("logo.png");
        auto data = readZipEntry(zip, deflated, plenty);
        check(data.has_value() && *data == picture, "a deflated entry inflates back to itself");
        check(deflated.compressed < deflated.uncompressed, "it really was compressed");
        check(!readZipEntry(zip, deflated, 16), "an entry over the limit is refused");

        auto stored = readZipEntry(zip, index->entries.at("nested/pic.ppm"), plenty);
        check(stored.has_value() && *stored == note, "a stored entry comes back as it went in");

        const auto bomb = makeZip({{"big.bin", std::vector<uint8_t>(4u << 20, 0), true}});
        auto bombIndex = readZipIndex(bomb);
        check(bombIndex && !readZipEntry(bomb, bombIndex->entries.at("big.bin"), plenty),
              "an entry that expands too far is refused even under the limit");

        check(!readZipIndex(bytesOf("PK not really")), "a short file is not a zip");
        check(!readZipIndex({}), "an empty file is not a zip");
        auto truncated = zip;
        truncated.resize(truncated.size() / 2);
        check(!readZipIndex(truncated), "a truncated zip is refused");
    }

    // tar: plain names, "./" in front, ustar prefixes, gnu and pax long names
    {
        const auto longName = "assets/" + std::string(120, 'l') + ".png";
        const auto tar = makeTar({{"./assets/icon80x80.png", picture}, {"notes.txt", note}, {longName, note}});
        auto index = readTarIndex(tar);
        check(index.has_value(), "a tar this test wrote reads back");
        if (index) {
            check(index->entries.contains("assets/icon80x80.png"), "a leading ./ is dropped from a tar name");
            check(index->entries.contains(longName), "a gnu long name is read from its own entry");
        }

        const auto pax = makeTar({{longName, picture}}, LongName::Pax);
        auto paxIndex = readTarIndex(pax);
        check(paxIndex && paxIndex->entries.contains(longName), "a pax path is read the same way");

        auto prefixed = tarHeader("icon.png", '0', note.size(), "deep/dir");
        padded(prefixed, note);
        prefixed.resize(prefixed.size() + 1024, 0);
        auto prefixIndex = readTarIndex(prefixed);
        check(prefixIndex && prefixIndex->entries.contains("deep/dir/icon.png"), "a ustar prefix goes in front of the name");

        auto damaged = tar;
        damaged[10] ^= 0xFF;
        check(!readTarIndex(damaged), "a header whose checksum fails is not a tar");
        check(!readTarIndex(std::vector<uint8_t>(512, 'x')), "512 bytes of anything are not a tar");
    }

    // ar: what an ipk is, with gnu and bsd names
    {
        const auto ar = makeAr({{"debian-binary", bytesOf("2.0\n")}, {"data.tar.gz", note}});
        auto index = readArIndex(ar);
        check(index && index->entries.size() == 2 && index->entries.contains("data.tar.gz"), "an ar lists its members by name");
        if (index) {
            const Archive opened{ar, *index};
            auto data = readEntry(opened, index->entries.at("data.tar.gz"), plenty);
            check(data && *data == note, "an ar member reads back as it went in, padding and all");
        }

        const auto bsd = makeAr({{"a-rather-long-member-name.png", note}}, true);
        auto bsdIndex = readArIndex(bsd);
        check(bsdIndex && bsdIndex->entries.contains("a-rather-long-member-name.png"), "a bsd #1/ name is read off the front of the member");
        check(!readArIndex(bytesOf("!<arch>\nbroken header")), "a damaged ar is refused");
    }

    // gzip: undone whole, and held to the same limits a zip entry is
    {
        auto undone = gunzip(gzipOf(picture), plenty);
        check(undone && *undone == picture, "a gzip undoes to what went in");
        check(!gunzip(gzipOf(picture), 100), "a gzip that grows past the limit is refused");
        check(!gunzip(gzipOf(std::vector<uint8_t>(4u << 20, 0)), plenty), "a gzip that expands too far is refused");
        auto cut = gzipOf(picture);
        cut.resize(cut.size() / 2);
        check(!gunzip(cut, plenty), "a gzip cut short is refused");
    }

    // the whole point: foo.ipk/data.tar.gz/assets/icon80x80.png
    {
        const auto data = gzipOf(makeTar({{"./assets/icon80x80.png", picture}, {"./assets/other.png", note}}));
        const auto control = gzipOf(makeTar({{"./control", bytesOf("Package: foo\n")}}));
        const auto ipk = makeAr({{"debian-binary", bytesOf("2.0\n")}, {"control.tar.gz", control}, {"data.tar.gz", data}});

        auto icon = dig(ipk, "foo.ipk", "data.tar.gz/assets/icon80x80.png");
        check(icon && icon->data == picture, "a file inside a tar.gz inside an ar is found");
        if (icon) check(icon->path == "assets/icon80x80.png", "the blob is named by the file, not by the archive");

        // an older opkg ipk is a tar.gz itself, and the bytes say so whatever the name
        const auto oldIpk = gzipOf(makeTar({{"./data.tar.gz", data}}));
        auto oldIcon = dig(oldIpk, "old.ipk", "data.tar.gz/assets/icon80x80.png");
        check(oldIcon && oldIcon->data == picture, "an ipk that is a tar.gz opens the same way");

        // a zip inside a zip used to be read as a file; now it is opened
        const auto inner = makeZip({{"logo.png", picture, true}});
        const auto outer = makeZip({{"inner.zip", inner, false}});
        auto nested = dig(outer, "outer.zip", "inner.zip/logo.png");
        check(nested && nested->data == picture, "an archive inside a zip is opened in turn");

        auto missing = dig(ipk, "foo.ipk", "data.tar.gz/assets/nope.png");
        check(!missing && missing.error().code == ErrorCode::NotFound, "a name that is not there is not found");
        auto notArchive = dig(ipk, "foo.ipk", "debian-binary/x");
        check(!notArchive, "a file that is not an archive cannot be walked into");
        check(!dig(bytesOf("plain bytes"), "x.zip", "a.png"), "bytes that are no archive are refused");

        // every level is kept, so the second icon costs no gunzip
        Archives archives(plenty);
        auto first = dig(ipk, "foo.ipk", "data.tar.gz/assets/icon80x80.png", &archives);
        check(first.has_value(), "digging with a cache works");
        check(archives.find("foo.ipk/data.tar.gz") != nullptr, "the tar.gz inside is kept under its own key");
    }

    // "pack.zip/logo.png" is an archive and a name, anything else is a plain path
    {
        const auto split = splitArchivePath("foo.ipk/data.tar.gz/assets/icon.png");
        check(split && split->archive == "foo.ipk" && split->inside == "data.tar.gz/assets/icon.png",
              "the first segment named like an archive is the file");
        const auto deep = splitArchivePath("a/b/pack.ZIP/dir/logo.png");
        check(deep && deep->archive == "a/b/pack.ZIP" && deep->inside == "dir/logo.png", "the extension is read in any case");
        const auto tgz = splitArchivePath("x/y.tgz/z.png");
        check(tgz && tgz->archive == "x/y.tgz", "a .tgz is an archive");

        check(!splitArchivePath("plain/logo.png"), "a path with no archive is not split");
        check(!splitArchivePath("pack.zip"), "an archive with nothing after it is not split");
        check(!splitArchivePath("pack.zip/"), "an archive with an empty name is not split");
    }

    // the archives kept in memory are bounded and least recently used goes first
    {
        const auto opened = [&] { return Archive{zip, *readZipIndex(zip)}; };
        Archives archives(zip.size() * 2);
        check(archives.find("a") == nullptr, "an unknown archive is a miss");

        archives.keep("a", opened());
        archives.keep("b", opened());
        check(archives.find("a") != nullptr, "both archives fit");

        archives.find("a");   // touching makes "b" the oldest
        archives.keep("c", opened());
        check(archives.find("a") != nullptr, "the recently used archive stays");
        check(archives.find("b") == nullptr, "the least recently used is evicted");

        Archives tiny(16);
        check(tiny.keep("big", opened()) != nullptr, "an oversized archive is still returned");
        check(tiny.find("big") == nullptr, "an oversized archive is not remembered");
    }

    if (failures == 0) std::cout << "source archive: ok\n";
    return failures == 0 ? 0 : 1;
}

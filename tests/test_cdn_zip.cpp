#include <internal/bucket.hpp>

#include <zlib.h>

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace Haio;
using namespace Haio::Cdn::Bucket;

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

}

auto main() -> int {
    const auto picture = bytesOf(std::string(4096, 'P'));
    const auto note = bytesOf("not an image");

    const auto zip = makeZip({{"logo.png", picture, true},
                              {"nested/pic.ppm", note, false},
                              {"notes.txt", note, true}});

    auto index = readZipIndex(zip);
    check(index.has_value(), "a zip this test wrote reads back");
    if (!index) return 1;

    check(index->entries.size() == 3, "every entry is indexed");
    check(index->entries.contains("nested/pic.ppm"), "a name with a directory in it is kept whole");

    // deflated
    {
        const auto found = index->entries.find("logo.png");
        check(found != index->entries.end(), "the deflated entry is there");
        auto data = readZipEntry(zip, found->second, plenty);
        check(data.has_value() && *data == picture, "a deflated entry inflates back to itself");
        check(found->second.compressed < found->second.uncompressed, "it really was compressed");
    }

    // stored
    {
        const auto found = index->entries.find("nested/pic.ppm");
        auto data = readZipEntry(zip, found->second, plenty);
        check(data.has_value() && *data == note, "a stored entry comes back as it went in");
    }

    // the limit that stands between a small file and a large allocation
    {
        const auto found = index->entries.find("logo.png");
        auto data = readZipEntry(zip, found->second, 16);
        check(!data, "an entry over max_unzip is refused");
    }

    // a ratio no picture ever has: 4mb of zeroes in a few hundred bytes
    {
        const auto bomb = makeZip({{"big.bin", std::vector<uint8_t>(4u << 20, 0), true}});
        auto index2 = readZipIndex(bomb);
        check(index2.has_value(), "the bomb is a valid zip");
        if (index2) {
            const auto found = index2->entries.find("big.bin");
            auto data = readZipEntry(bomb, found->second, plenty);
            check(!data, "an entry that expands too far is refused even under max_unzip");
        }
    }

    // nothing that is not a zip may look like one
    check(!readZipIndex(bytesOf("PK not really")), "a short file is not a zip");
    check(!readZipIndex({}), "an empty file is not a zip");
    {
        auto truncated = zip;
        truncated.resize(truncated.size() / 2);
        check(!readZipIndex(truncated), "a truncated zip is refused");
    }

    // "pack.zip/logo.png" is an archive and a name, anything else is a plain path
    {
        const auto split = splitZipPath("pack.zip/logo.png");
        check(split.has_value(), "a zip path is recognised");
        if (split) {
            check(split->archive == "pack.zip", "the archive is everything up to .zip");
            check(split->inside == "logo.png", "the rest names the file inside");
        }
        const auto deep = splitZipPath("a/b/pack.zip/dir/logo.png");
        check(deep && deep->archive == "a/b/pack.zip" && deep->inside == "dir/logo.png",
              "a zip deeper in the tree splits the same way");

        check(!splitZipPath("plain/logo.png"), "a path with no zip is not split");
        check(!splitZipPath("pack.zip"), "an archive with nothing after it is not split");
        check(!splitZipPath("pack.zip/"), "an archive with an empty name is not split");
    }

    // the archives kept in memory are bounded and least recently used goes first
    {
        ZipArchives archives(zip.size() * 2);
        check(archives.find("a") == nullptr, "an unknown archive is a miss");

        archives.keep("a", zip, *index);
        archives.keep("b", zip, *index);
        check(archives.find("a") != nullptr, "both archives fit");

        archives.find("a");                    // touching makes "b" the oldest
        archives.keep("c", zip, *index);
        check(archives.find("a") != nullptr, "the recently used archive stays");
        check(archives.find("b") == nullptr, "the least recently used is evicted");

        // one too big to keep is still handed back, it just is not remembered
        ZipArchives tiny(16);
        const auto held = tiny.keep("big", zip, *index);
        check(held != nullptr, "an oversized archive is still returned");
        check(tiny.find("big") == nullptr, "an oversized archive is not remembered");
    }

    if (failures == 0) std::cout << "cdn zip: ok\n";
    return failures == 0 ? 0 : 1;
}

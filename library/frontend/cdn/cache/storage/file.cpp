#include <haio_cache.hpp>

#include <boost/asio/use_awaitable.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>
#include <vector>

namespace asio = boost::asio;

namespace {

using FileClock = std::filesystem::file_time_type::clock;

/**
 * one file per key, named by the md5, so nothing in a request ever reaches the file
 * system as a path. writes land on a neighbouring ".tmp" and are renamed into place,
 * because a reader must never see half an entry.
 *
 * the modification time is the expiry clock, and a hit touches it: an entry that
 * keeps being asked for keeps living, and one nobody wants ages out. that also means
 * the sweep can order by mtime and be evicting the least recently *used*.
 */
class FileStore final : public Haio::Cdn::CacheStore {
public:
    FileStore(std::filesystem::path root, std::chrono::seconds ttl, size_t maxUsage)
        : root_(std::move(root)), ttl_(ttl), maxUsage_(maxUsage) {
        std::error_code ec;
        std::filesystem::create_directories(root_, ec);
        held_ = measure();
    }

    asio::awaitable<std::optional<Haio::Cdn::CacheEntry>> get(const std::string& key) override {
        const auto path = root_ / key;

        std::error_code ec;
        const auto written = std::filesystem::last_write_time(path, ec);
        if (ec) co_return std::nullopt;

        if (FileClock::now() - written >= ttl_) {
            forget(path);
            co_return std::nullopt;
        }

        std::ifstream in(path, std::ios::binary);
        if (!in) co_return std::nullopt;

        const std::vector<uint8_t> raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        auto entry = Haio::Cdn::decodeEntry(raw);
        if (!entry) {
            // a file that cannot be read back is worse than no file at all
            forget(path);
            co_return std::nullopt;
        }

        // renew, so the ttl measures time since the last read rather than the write
        std::filesystem::last_write_time(path, FileClock::now(), ec);
        co_return entry;
    }

    asio::awaitable<void> put(const std::string& key, const Haio::Cdn::CacheEntry& entry) override {
        const auto raw = Haio::Cdn::encodeEntry(entry);
        if (raw.size() > maxUsage_) co_return;   // one entry that cannot fit never evicts the rest

        const auto path = root_ / key;
        const auto pending = root_ / (key + ".tmp");

        std::error_code ec;
        const auto replacing = std::filesystem::file_size(path, ec);
        const auto previous = ec ? size_t{0} : static_cast<size_t>(replacing);

        {
            std::ofstream out(pending, std::ios::binary | std::ios::trunc);
            if (!out) co_return;
            out.write(reinterpret_cast<const char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
            if (!out) {
                out.close();
                std::filesystem::remove(pending, ec);
                co_return;
            }
        }

        std::filesystem::rename(pending, path, ec);
        if (ec) {
            std::filesystem::remove(pending, ec);
            co_return;
        }

        held_ = held_ - previous + raw.size();
        if (held_ > maxUsage_) sweep();
        co_return;
    }

private:
    /** the directory is the state, so its size is read from it rather than remembered */
    size_t measure() const {
        size_t total = 0;
        std::error_code ec;
        for (const auto& item : std::filesystem::directory_iterator(root_, ec)) {
            std::error_code each;
            if (const auto size = item.file_size(each); !each) total += static_cast<size_t>(size);
        }
        return total;
    }

    void forget(const std::filesystem::path& path) {
        std::error_code ec;
        const auto size = std::filesystem::file_size(path, ec);
        if (!ec && !std::filesystem::remove(path, ec)) return;
        if (!ec) held_ -= std::min(held_, static_cast<size_t>(size));
    }

    /**
     * expired entries go first, and then the oldest until the budget is met. without
     * this the directory only ever grows: an entry written once and never read again
     * is never asked for, so nothing would ever notice it had expired.
     */
    void sweep() {
        struct Aged {
            std::filesystem::path file;   // not "path": that reads like directory_entry::path()
            std::filesystem::file_time_type written;
            size_t size;
        };

        std::vector<Aged> kept;
        std::error_code ec;
        const auto now = FileClock::now();

        for (const auto& item : std::filesystem::directory_iterator(root_, ec)) {
            std::error_code each;
            const auto written = item.last_write_time(each);
            if (each) continue;
            const auto size = item.file_size(each);
            if (each) continue;

            // a .tmp left behind by a crash has nobody coming back for it
            if (item.path().extension() == ".tmp" || now - written >= ttl_) {
                std::error_code drop;
                std::filesystem::remove(item.path(), drop);
                continue;
            }
            kept.push_back(Aged{item.path(), written, static_cast<size_t>(size)});
        }

        size_t total = 0;
        for (const auto& item : kept) total += item.size;

        if (total > maxUsage_) {
            std::ranges::sort(kept, {}, &Aged::written);   // least recently used first
            for (const auto& item : kept) {
                if (total <= maxUsage_) break;
                std::error_code drop;
                if (std::filesystem::remove(item.file, drop)) total -= item.size;
            }
        }
        held_ = total;
    }

    std::filesystem::path root_;
    std::chrono::seconds ttl_;
    size_t maxUsage_;
    size_t held_ = 0;
};

}

namespace Haio::Cdn {

std::unique_ptr<CacheStore> makeFileStore(std::filesystem::path root, std::chrono::seconds ttl, size_t maxUsage) {
    return std::make_unique<FileStore>(std::move(root), ttl, maxUsage);
}

}

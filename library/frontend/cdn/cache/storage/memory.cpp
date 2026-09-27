#include <haio_cache.hpp>
#include <haio/internal/utils/lru.hpp>

#include <chrono>

namespace {

using Clock = std::chrono::steady_clock;

/**
 * least recently used, bounded by the total size of the bodies it holds, with a ttl
 * on top that a hit renews.
 */
class MemoryStore final : public Haio::Cdn::CacheStore {
public:
    MemoryStore(size_t maxUsage, std::chrono::seconds ttl) : held_(maxUsage), ttl_(ttl) {}

    Haio::Task<std::optional<Haio::Cdn::CacheEntry>> get(const std::string& key) override {
        auto* found = held_.find(key);
        if (!found) co_return std::nullopt;

        if (Clock::now() >= found->storedUntil) {
            held_.erase(key);
            co_return std::nullopt;
        }

        // a hit is both a promotion and a renewal: asking for something keeps it
        found->storedUntil = Clock::now() + ttl_;
        co_return found->entry;
    }

    Haio::Task<void> put(const std::string& key, const Haio::Cdn::CacheEntry& entry) override {
        const auto size = entry.data.size() + entry.contentType.size() + entry.filename.size();
        held_.keep(key, Stored{entry, Clock::now() + ttl_}, size);
        co_return;
    }

private:
    struct Stored {
        Haio::Cdn::CacheEntry entry;
        Clock::time_point storedUntil;
    };

    Haio::Detail::LruBySize<Stored> held_;
    std::chrono::seconds ttl_;
};

}

namespace Haio::Cdn {

std::unique_ptr<CacheStore> makeMemoryStore(size_t maxUsage, std::chrono::seconds ttl) {
    return std::make_unique<MemoryStore>(maxUsage, ttl);
}

}

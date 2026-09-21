#include <haio_cache.hpp>

#include <boost/asio/use_awaitable.hpp>

#include <chrono>
#include <list>
#include <unordered_map>

namespace asio = boost::asio;

namespace {

using Clock = std::chrono::steady_clock;

/**
 * least recently used, bounded by the total size of the bodies it holds. the bound is
 * on bytes rather than on entries because one creative can be a thousand times the
 * size of another, and a count would let a handful of them take the whole box.
 */
class MemoryStore final : public Haio::Cdn::CacheStore {
public:
    MemoryStore(size_t maxUsage, std::chrono::seconds ttl) : maxUsage_(maxUsage), ttl_(ttl) {}

    asio::awaitable<std::optional<Haio::Cdn::CacheEntry>> get(const std::string& key) override {
        const auto found = index_.find(key);
        if (found == index_.end()) co_return std::nullopt;

        if (Clock::now() >= found->second->storedUntil) {
            drop(found);
            co_return std::nullopt;
        }

        // a hit is both a promotion and a renewal: asking for something keeps it
        order_.splice(order_.begin(), order_, found->second);
        found->second->storedUntil = Clock::now() + ttl_;
        co_return found->second->entry;
    }

    asio::awaitable<void> put(const std::string& key, const Haio::Cdn::CacheEntry& entry) override {
        const auto size = sizeOf(entry);
        if (size > maxUsage_) co_return;   // one entry that cannot fit never evicts the rest

        if (const auto found = index_.find(key); found != index_.end()) drop(found);

        order_.push_front(Node{key, entry, Clock::now() + ttl_, size});
        index_.emplace(key, order_.begin());
        held_ += size;

        while (held_ > maxUsage_ && !order_.empty()) {
            const auto last = std::prev(order_.end());
            held_ -= last->size;
            index_.erase(last->key);
            order_.erase(last);
        }
        co_return;
    }

private:
    struct Node {
        std::string key;
        Haio::Cdn::CacheEntry entry;
        Clock::time_point storedUntil;
        size_t size = 0;
    };

    static size_t sizeOf(const Haio::Cdn::CacheEntry& entry) {
        return entry.data.size() + entry.contentType.size() + entry.filename.size();
    }

    void drop(std::unordered_map<std::string, std::list<Node>::iterator>::iterator found) {
        held_ -= found->second->size;
        order_.erase(found->second);
        index_.erase(found);
    }

    size_t maxUsage_;
    std::chrono::seconds ttl_;
    size_t held_ = 0;
    std::list<Node> order_;
    std::unordered_map<std::string, std::list<Node>::iterator> index_;
};

}

namespace Haio::Cdn {

std::unique_ptr<CacheStore> makeMemoryStore(size_t maxUsage, std::chrono::seconds ttl) {
    return std::make_unique<MemoryStore>(maxUsage, ttl);
}

}

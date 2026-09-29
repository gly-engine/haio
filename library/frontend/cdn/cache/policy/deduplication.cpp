#include <haio/internal/cdn/cache.hpp>

namespace Haio::Cdn::Detail {

/**
 * the answer lives here rather than being read back from a store, so waiters are
 * served even when nothing is being stored.
 */
struct SingleFlight::Pending {
    Event finished;
    std::optional<Result<CacheEntry>> answer;
};

/**
 * the map carries no lock because every platform drives its loop from one thread, so
 * a coroutine only ever yields at a co_await and never mid update. putting the server
 * on a thread pool means revisiting exactly this.
 */
Task<Result<CacheEntry>> SingleFlight::run(const std::string& key, const Producer& produce) {
    // looked up again after every wait: the leader may have left without an answer,
    // and then the first waiter to wake takes over while the rest wait on it instead
    while (true) {
        const auto waiting = inFlight_.find(key);
        if (waiting == inFlight_.end()) break;

        const auto pending = waiting->second;
        co_await pending->finished;
        if (pending->answer) co_return *pending->answer;
    }

    auto pending = std::make_shared<Pending>();
    inFlight_.emplace(key, pending);

    /**
     * the waiters have to be woken however this ends, including by an exception, or
     * every one of them parks forever. the answer is written before the wake, so a
     * waiter that runs the instant it is woken already has something to read.
     */
    struct Wake {
        SingleFlight& owner;
        std::string key;
        std::shared_ptr<Pending> pending;
        ~Wake() {
            owner.inFlight_.erase(key);
            pending->finished.set();
        }
    } wake{*this, key, pending};

    auto made = co_await produce();
    pending->answer = made;
    co_return made;
}

}

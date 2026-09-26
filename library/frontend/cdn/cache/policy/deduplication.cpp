#include <internal/cache.hpp>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace asio = boost::asio;

namespace Haio::Cdn::Detail {

/**
 * the answer lives here rather than being read back from a store, so waiters are
 * served even when nothing is being stored.
 */
struct SingleFlight::Pending {
    std::shared_ptr<asio::steady_timer> timer;
    std::optional<Result<CacheEntry>> answer;
};

SingleFlight::SingleFlight(asio::any_io_executor executor) : executor_(std::move(executor)) {}

/**
 * the map carries no lock because the cdn runs one io_context on one thread, so a
 * coroutine only ever yields at a co_await and never mid update. putting the server
 * on a thread pool means revisiting exactly this.
 */
asio::awaitable<Result<CacheEntry>> SingleFlight::run(const std::string& key, const Producer& produce) {
    if (const auto waiting = inFlight_.find(key); waiting != inFlight_.end()) {
        auto pending = waiting->second;
        co_await pending->timer->async_wait(asio::as_tuple(asio::use_awaitable));
        if (pending->answer) co_return *pending->answer;
        // the leader left without an answer, so this caller does the work instead
    }

    auto pending = std::make_shared<Pending>();
    pending->timer = std::make_shared<asio::steady_timer>(executor_, asio::steady_timer::time_point::max());
    inFlight_.emplace(key, pending);

    /**
     * the timer has to be cancelled however this ends, including by an exception, or
     * every waiter parks forever. the answer is written before the wake, so a waiter
     * that runs the instant it is cancelled already has something to read.
     */
    struct Wake {
        SingleFlight& owner;
        std::string key;
        std::shared_ptr<Pending> pending;
        ~Wake() {
            owner.inFlight_.erase(key);
            pending->timer->cancel();
        }
    } wake{*this, key, pending};

    auto made = co_await produce();
    pending->answer = made;
    co_return made;
}

}

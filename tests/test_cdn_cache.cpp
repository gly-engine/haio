#include <haio_cache.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace asio = boost::asio;
using namespace Haio;
using namespace Haio::Cdn;

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (ok) return;
    std::cerr << "fail: " << what << '\n';
    failures++;
}

CacheEntry entryOf(std::string body) {
    return CacheEntry{{body.begin(), body.end()}, "image/png", "x.png"};
}

std::string bodyOf(const CacheEntry& entry) {
    return {entry.data.begin(), entry.data.end()};
}

/** the key is what decides whether two requests are the same request */
void testKeyNormalisation() {
    // the query spells a pipeline, so its order is meaning and not decoration: two
    // orders are two pictures and must never share an entry
    check(cacheKey("b", "/a.png", "crop=1,1,2,2&resize=10") != cacheKey("b", "/a.png", "resize=10&crop=1,1,2,2"),
          "query order changes the key, because it changes the result");
    check(cacheKey("b", "/a.png", "w=10") == cacheKey("b", "/a.png", "w=10"),
          "the same query gives the same key");
    check(cacheKey("b", "/a.png", "w=10") != cacheKey("b", "/a.png", "w=11"),
          "a different value is a different key");
    check(cacheKey("b", "/a.png", "") != cacheKey("c", "/a.png", ""),
          "the bucket is part of the key");
    check(cacheKey("b", "/a.png", "") != cacheKey("b", "/b.png", ""),
          "the path is part of the key");
    check(cacheKey("b", "/a.png", "").size() == 32, "the key is an md5 in hex");
}

/** an entry crosses a socket or a disk as bytes, and has to come back unchanged */
void testFraming() {
    const CacheEntry entry{{0x00, 0xFF, 0x10, 0x00}, "image/x-zcis", "name with spaces.zcis"};
    const auto decoded = decodeEntry(encodeEntry(entry));
    check(decoded.has_value(), "an encoded entry decodes");
    if (decoded) {
        check(decoded->data == entry.data, "the body survives, nulls and all");
        check(decoded->contentType == entry.contentType, "the content type survives");
        check(decoded->filename == entry.filename, "the filename survives");
    }

    // a half written file must read as a miss, never as a broken response
    auto raw = encodeEntry(entry);
    raw.resize(raw.size() / 2);
    check(!decodeEntry(raw).has_value(), "a truncated entry is a miss");
    check(!decodeEntry({}).has_value(), "an empty entry is a miss");
}

asio::awaitable<void> testMemoryStore() {
    auto store = makeMemoryStore(1024, std::chrono::seconds(60));

    check(!(co_await store->get("missing")).has_value(), "an unknown key misses");
    co_await store->put("a", entryOf("first"));

    auto hit = co_await store->get("a");
    check(hit.has_value() && bodyOf(*hit) == "first", "what went in comes back");

    // an entry costs its body plus its content type and filename, so the bound is
    // written from that rather than guessed: room for two of these and not three
    constexpr size_t body = 30;
    const size_t each = body + std::string("image/png").size() + std::string("x.png").size();

    auto small = makeMemoryStore(each * 2, std::chrono::seconds(60));
    co_await small->put("one", entryOf(std::string(body, 'x')));
    co_await small->put("two", entryOf(std::string(body, 'y')));
    check((co_await small->get("one")).has_value(), "both entries fit under the bound");

    co_await small->get("one");                               // touching makes "two" the oldest
    co_await small->put("three", entryOf(std::string(body, 'z')));

    check((co_await small->get("one")).has_value(), "the recently used entry stays");
    check(!(co_await small->get("two")).has_value(), "the least recently used is evicted");
    check((co_await small->get("three")).has_value(), "the newcomer is kept");

    // an entry too big for the cache must not evict everything else on its way in
    auto tiny = makeMemoryStore(each * 2, std::chrono::seconds(60));
    co_await tiny->put("keep", entryOf(std::string(body, 'k')));
    co_await tiny->put("huge", entryOf(std::string(each * 4, 'h')));
    check((co_await tiny->get("keep")).has_value(), "an oversized entry does not flush the cache");
    check(!(co_await tiny->get("huge")).has_value(), "an oversized entry is not stored");
}

asio::awaitable<void> testTtlExpires() {
    auto store = makeMemoryStore(1024, std::chrono::seconds(0));
    co_await store->put("a", entryOf("gone"));
    check(!(co_await store->get("a")).has_value(), "a zero ttl entry is already stale");
}

/**
 * the one that matters: a burst on a cold key must reach the producer once. without
 * this every caller in the burst fetches the upstream, which on an open bucket points
 * an amplifier at somebody else's server.
 */
asio::awaitable<void> testSingleFlight(asio::any_io_executor executor) {
    CacheConfig config;
    config.ttl = std::chrono::seconds(60);
    Cache cache{config, 0, executor};
    check(cache.enabled(), "a cache with a ttl is on");

    int produced = 0;
    auto slowProducer = [&produced, executor]() -> asio::awaitable<Result<CacheEntry>> {
        produced++;
        asio::steady_timer wait(executor, std::chrono::milliseconds(50));
        co_await wait.async_wait(asio::use_awaitable);
        co_return entryOf("shared");
    };

    constexpr int callers = 8;
    int answered = 0;
    for (int i = 0; i < callers; i++) {
        asio::co_spawn(executor, [&]() -> asio::awaitable<void> {
            auto got = co_await cache.fetch("hot", "10.0.0.1", slowProducer);
            if (got && bodyOf(*got) == "shared") answered++;
        }, asio::detached);
    }

    asio::steady_timer settle(executor, std::chrono::milliseconds(300));
    co_await settle.async_wait(asio::use_awaitable);

    check(answered == callers, "every caller in the burst is answered");
    check(produced == 1, "the producer ran once for the whole burst, not " + std::to_string(produced) + " times");
}

/** a cache with no ttl configured must stay out of the way entirely */
asio::awaitable<void> testDisabledCachePassesThrough() {
    Cache off;
    check(!off.enabled(), "a default cache is off");

    int produced = 0;
    for (int i = 0; i < 3; i++) {
        auto got = co_await off.fetch("k", "10.0.0.1", [&produced]() -> asio::awaitable<Result<CacheEntry>> {
            produced++;
            co_return entryOf("live");
        });
        check(got.has_value(), "a disabled cache still answers");
    }
    check(produced == 3, "a disabled cache never stores, so every call produces");
}

/** a producer that fails must not be stored, and must not poison the next caller */
asio::awaitable<void> testFailureIsNotCached(asio::any_io_executor executor) {
    CacheConfig config;
    config.ttl = std::chrono::seconds(60);
    Cache cache{config, 0, executor};

    auto failing = []() -> asio::awaitable<Result<CacheEntry>> {
        co_return std::unexpected(Error{ErrorCode::Upstream, "upstream said no"});
    };
    auto failed = co_await cache.fetch("k", "10.0.0.1", failing);
    check(!failed, "a failed produce is reported");

    auto ok = co_await cache.fetch("k", "10.0.0.1", []() -> asio::awaitable<Result<CacheEntry>> {
        co_return entryOf("second try");
    });
    check(ok && bodyOf(*ok) == "second try", "the failure was not stored");
}

/**
 * deduplication is not part of caching: with no ttl nothing is kept, and a burst on
 * one key must still reach the producer once. otherwise turning the cache off would
 * quietly turn an amplifier back on.
 */
asio::awaitable<void> testDedupWithoutCache(asio::any_io_executor executor) {
    CacheConfig config;
    config.ttl = std::chrono::seconds(0);   // keeps nothing
    Cache cache{config, 0, executor};
    check(!cache.enabled(), "a zero ttl keeps nothing");

    int produced = 0;
    auto slow = [&produced, executor]() -> asio::awaitable<Result<CacheEntry>> {
        produced++;
        asio::steady_timer wait(executor, std::chrono::milliseconds(50));
        co_await wait.async_wait(asio::use_awaitable);
        co_return entryOf("shared");
    };

    constexpr int callers = 6;
    int answered = 0;
    for (int i = 0; i < callers; i++) {
        asio::co_spawn(executor, [&]() -> asio::awaitable<void> {
            auto got = co_await cache.fetch("cold", "10.0.0.2", slow);
            if (got && bodyOf(*got) == "shared") answered++;
        }, asio::detached);
    }

    asio::steady_timer settle(executor, std::chrono::milliseconds(300));
    co_await settle.async_wait(asio::use_awaitable);

    check(answered == callers, "every caller in the burst is answered");
    check(produced == 1, "one producer served the burst, not " + std::to_string(produced));

    // and nothing was kept, so the next burst produces again
    auto after = co_await cache.fetch("cold", "10.0.0.2", slow);
    check(after.has_value() && produced == 2, "with no ttl the answer is not kept");
}

/** the store that outlives the process: sweeping and renewal are the whole point */
asio::awaitable<void> testFileStore() {
    const auto root = std::filesystem::temp_directory_path() / "haio_cache_test";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);

    {
        auto store = makeFileStore(root, std::chrono::seconds(60), 1024 * 1024);
        co_await store->put("aaa", entryOf("written"));

        auto hit = co_await store->get("aaa");
        check(hit.has_value() && bodyOf(*hit) == "written", "an entry survives the round trip to disk");
        check(!(co_await store->get("bbb")).has_value(), "an unknown key misses");
    }

    // a fresh store reads what the last one left, which is the reason file:// exists
    {
        auto reopened = makeFileStore(root, std::chrono::seconds(60), 1024 * 1024);
        auto hit = co_await reopened->get("aaa");
        check(hit.has_value() && bodyOf(*hit) == "written", "entries survive a restart");
    }

    // a corrupt file must read as a miss and take itself out of the way
    {
        std::ofstream(root / "ccc", std::ios::binary) << "not an entry";
        auto store = makeFileStore(root, std::chrono::seconds(60), 1024 * 1024);
        check(!(co_await store->get("ccc")).has_value(), "a corrupt file is a miss");
        check(!std::filesystem::exists(root / "ccc"), "a corrupt file is removed rather than left");
    }

    // the sweep is what keeps the directory from being written to forever
    {
        std::filesystem::remove_all(root, ec);
        const auto body = std::string(4096, 'x');
        const auto each = encodeEntry(entryOf(body)).size();

        auto store = makeFileStore(root, std::chrono::seconds(60), each * 4);
        for (int i = 0; i < 12; i++) {
            co_await store->put("k" + std::to_string(i), entryOf(body));
        }

        size_t onDisk = 0;
        for (const auto& item : std::filesystem::directory_iterator(root)) {
            std::error_code each2;
            if (const auto size = item.file_size(each2); !each2) onDisk += static_cast<size_t>(size);
        }
        check(onDisk <= each * 4, "the sweep holds the directory under its budget");
        check(onDisk > 0, "the sweep does not empty the directory either");

        // whatever survived has to still be readable, not just small
        int readable = 0;
        for (int i = 0; i < 12; i++) {
            if ((co_await store->get("k" + std::to_string(i))).has_value()) readable++;
        }
        check(readable > 0, "what the sweep kept is still usable");
    }

    // an entry larger than the whole budget is refused instead of flushing the rest
    {
        std::filesystem::remove_all(root, ec);
        auto store = makeFileStore(root, std::chrono::seconds(60), 512);
        co_await store->put("keep", entryOf("small"));
        co_await store->put("huge", entryOf(std::string(4096, 'h')));
        check((co_await store->get("keep")).has_value(), "an oversized entry does not flush the cache");
        check(!(co_await store->get("huge")).has_value(), "an oversized entry is not stored");
    }

    // expiry is measured from the last read, so an entry past its ttl is gone
    {
        std::filesystem::remove_all(root, ec);
        auto store = makeFileStore(root, std::chrono::seconds(0), 1024 * 1024);
        co_await store->put("old", entryOf("stale"));
        check(!(co_await store->get("old")).has_value(), "a zero ttl entry is already stale");
    }

    std::filesystem::remove_all(root, ec);
}

/**
 * the quota is about eviction, not about access: going over must still answer, and
 * must still serve what is already stored. only the storing stops.
 */
asio::awaitable<void> testPerClientQuota(asio::any_io_executor executor) {
    CacheConfig config;
    config.ttl = std::chrono::seconds(60);
    Cache cache{config, 2, executor};

    auto make = [](std::string body) {
        return [body]() -> asio::awaitable<Result<CacheEntry>> { co_return entryOf(body); };
    };

    for (int i = 0; i < 4; i++) {
        auto got = co_await cache.fetch("k" + std::to_string(i), "10.0.0.9", make("body"));
        check(got.has_value(), "a caller over quota is still answered");
    }

    // the first two were stored, and the rest were produced and dropped
    int stored = 0;
    for (int i = 0; i < 4; i++) {
        int produced = 0;
        co_await cache.fetch("k" + std::to_string(i), "10.0.0.9",
                             [&produced]() -> asio::awaitable<Result<CacheEntry>> {
                                 produced++;
                                 co_return entryOf("again");
                             });
        if (produced == 0) stored++;
    }
    check(stored == 2, "only the quota's worth was stored, not " + std::to_string(stored));

    // another address has its own quota, and reading never costs anyone anything
    auto other = co_await cache.fetch("k0", "10.0.0.10", make("unused"));
    check(other && bodyOf(*other) == "body", "a different address reads what is stored");

    // no quota configured means no limit
    CacheConfig open;
    open.ttl = std::chrono::seconds(60);
    Cache free{open, 0, executor};
    for (int i = 0; i < 5; i++) co_await free.fetch("f" + std::to_string(i), "10.0.0.11", make("x"));
    int producedAgain = 0;
    co_await free.fetch("f4", "10.0.0.11", [&producedAgain]() -> asio::awaitable<Result<CacheEntry>> {
        producedAgain++;
        co_return entryOf("x");
    });
    check(producedAgain == 0, "with no quota every entry is stored");
}

}

auto main() -> int {
    testKeyNormalisation();
    testFraming();

    asio::io_context io;
    asio::co_spawn(io, [&]() -> asio::awaitable<void> {
        const auto executor = co_await asio::this_coro::executor;
        co_await testMemoryStore();
        co_await testTtlExpires();
        co_await testDisabledCachePassesThrough();
        co_await testFailureIsNotCached(executor);
        co_await testSingleFlight(executor);
        co_await testPerClientQuota(executor);
        co_await testFileStore();
        co_await testDedupWithoutCache(executor);
    }, asio::detached);
    io.run();

    if (failures == 0) std::cout << "cdn cache: ok\n";
    return failures == 0 ? 0 : 1;
}

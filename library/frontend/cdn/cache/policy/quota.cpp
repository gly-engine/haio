#include <internal/cache.hpp>

#include <algorithm>

namespace Haio::Cdn::Detail {

ClientQuota::ClientQuota(size_t perWindow, std::chrono::seconds window)
    : perWindow_(perWindow), window_(window) {}

/**
 * the quota counts entries created, not requests served, and it refills every window.
 * going over does not fail anything: the answer is produced and returned, it is only
 * not stored, so an address that asks for ten thousand different sizes pays for them
 * itself instead of evicting everybody else.
 */
bool ClientQuota::mayStore(const std::string& client) {
    if (perWindow_ == 0 || client.empty()) return true;

    const auto now = std::chrono::steady_clock::now();

    /**
     * the table is keyed by address, so a caller with many addresses grows it. expired
     * rows are dropped on the way past, and if it still runs away the whole table is
     * cleared: everyone gets a fresh quota, which is far better than an allocation
     * nobody bounds.
     *
     * @todo a fixed size counting sketch would hold the limit without the table, and
     * without handing an attacker a reset by flooding it.
     */
    if (seen_.size() > clientsBeforePrune) {
        std::erase_if(seen_, [now](const auto& row) { return now >= row.second.resets; });
        if (seen_.size() > clientsBeforePrune) seen_.clear();
    }

    auto& window = seen_[client];
    if (now >= window.resets) {
        window.created = 0;
        window.resets = now + window_;
    }
    if (window.created >= perWindow_) return false;

    window.created++;
    return true;
}

}

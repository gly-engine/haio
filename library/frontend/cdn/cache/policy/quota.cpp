#include <haio/internal/cdn/cache.hpp>


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

    // a window that has run out says nothing a fresh one would not
    auto& window = seen_.row(client, Window{}, [now](const Window& old) { return now >= old.resets; });
    if (now >= window.resets) {
        window.created = 0;
        window.resets = now + window_;
    }
    if (window.created >= perWindow_) return false;

    window.created++;
    return true;
}

}

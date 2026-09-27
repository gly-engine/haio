#include <internal/bucket.hpp>

namespace Haio::Cdn::Bucket {

/** the split is on the first ".zip/", so the outer archive is the one that opens */
std::optional<ZipPath> splitZipPath(std::string_view path) {
    constexpr std::string_view marker = ".zip/";

    const auto at = path.find(marker);
    if (at == std::string_view::npos) return std::nullopt;

    const auto inside = path.substr(at + marker.size());
    if (inside.empty()) return std::nullopt;

    return ZipPath{std::string(path.substr(0, at + 4)), std::string(inside)};
}

ZipArchives::ZipArchives(size_t maxUsage) : maxUsage_(maxUsage) {}

std::shared_ptr<const ZipArchives::Archive> ZipArchives::find(const std::string& key) {
    const auto found = index_.find(key);
    if (found == index_.end()) return nullptr;

    order_.splice(order_.begin(), order_, found->second);
    return found->second->archive;
}

std::shared_ptr<const ZipArchives::Archive> ZipArchives::keep(const std::string& key, std::vector<uint8_t> data, ZipIndex index) {
    const auto size = data.size();
    auto archive = std::make_shared<Archive>(Archive{std::move(data), std::move(index)});

    // an archive too big to keep is still handed back, it just is not remembered
    if (size > maxUsage_) return archive;

    if (const auto found = index_.find(key); found != index_.end()) {
        held_ -= found->second->size;
        order_.erase(found->second);
        index_.erase(found);
    }

    order_.push_front(Held{key, archive, size});
    index_.emplace(key, order_.begin());
    held_ += size;

    while (held_ > maxUsage_ && !order_.empty()) {
        const auto last = std::prev(order_.end());
        held_ -= last->size;
        index_.erase(last->key);
        order_.erase(last);
    }
    return archive;
}

}

#pragma once

#include <cstddef>
#include <iterator>
#include <list>
#include <string>
#include <unordered_map>
#include <utility>

namespace Haio::Detail {

/**
 * least recently used, bounded by what the values weigh rather than by how many there
 * are: one picture can be a thousand times the size of another, and a count would let
 * a handful of them take the whole box.
 *
 * the cache's memory store and the zip archives both keep things this way, and used
 * to each carry their own copy of the list, the index and the arithmetic.
 */
template <typename Value>
class LruBySize {
public:
    explicit LruBySize(size_t budget) : budget_(budget) {}

    /** the value, promoted to most recently used; nullptr when it is not here */
    Value* find(const std::string& key) {
        const auto found = index_.find(key);
        if (found == index_.end()) return nullptr;
        order_.splice(order_.begin(), order_, found->second);
        return &found->second->value;
    }

    /**
     * keeps the value and evicts the oldest until it fits. a value too big for the
     * whole budget is not kept, so it never flushes everything else on its way in.
     */
    void keep(const std::string& key, Value value, size_t size) {
        if (size > budget_) return;
        erase(key);

        order_.push_front(Node{key, std::move(value), size});
        index_.emplace(key, order_.begin());
        held_ += size;

        while (held_ > budget_ && !order_.empty()) {
            const auto last = std::prev(order_.end());
            held_ -= last->size;
            index_.erase(last->key);
            order_.erase(last);
        }
    }

    void erase(const std::string& key) {
        const auto found = index_.find(key);
        if (found == index_.end()) return;
        held_ -= found->second->size;
        order_.erase(found->second);
        index_.erase(found);
    }

private:
    struct Node {
        std::string key;
        Value value;
        size_t size = 0;
    };

    size_t budget_;
    size_t held_ = 0;
    std::list<Node> order_;
    std::unordered_map<std::string, typename std::list<Node>::iterator> index_;
};

}

#pragma once

#include <algorithm>
#include <cstddef>
#include <map>
#include <string>

namespace Haio::Cdn::Detail {

/**
 * one row per address, for the limits the cdn counts per caller: the request rate and
 * the cache quota. both used to carry their own copy of this.
 *
 * the table is keyed by address, so a caller with many addresses grows it. rows the
 * limit no longer cares about are dropped on the way past, and if it still runs away
 * the whole table is cleared: everyone starts fresh, which is far better than an
 * allocation nobody bounds.
 *
 * @todo a fixed size counting sketch would hold the limits without the table, and
 * without handing an attacker a reset by flooding it.
 */
template <typename Row>
class ClientTable {
public:
    /** the client's row, made as fresh when it has none; forgettable says which rows can go */
    template <typename Forgettable>
    Row& row(const std::string& client, const Row& fresh, Forgettable forgettable) {
        if (rows_.size() > clientsBeforePrune) {
            std::erase_if(rows_, [&](const auto& entry) { return forgettable(entry.second); });
            if (rows_.size() > clientsBeforePrune) rows_.clear();
        }
        return rows_.try_emplace(client, fresh).first->second;
    }

private:
    static constexpr size_t clientsBeforePrune = 10000;
    std::map<std::string, Row> rows_;
};

}

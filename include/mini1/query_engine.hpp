#pragma once

#include "mini1/interfaces.hpp"

#include <vector>

namespace mini1 {

// Generic range scan over any numeric field — templated on the accessor.
template <typename Accessor>
std::vector<std::size_t> range_scan(const IDataStore& store,
                                    Accessor accessor,
                                    double low, double high,
                                    bool inclusive)
{
    const std::size_t n = store.row_count();
    std::vector<std::size_t> result;
    result.reserve(n / 100);

    for (std::size_t i = 0; i < n; ++i) {
        const auto& row = store.row_at(i);
        if (!row.valid) continue;

        double val = static_cast<double>(accessor(row));
        bool hit = inclusive ? (val >= low && val <= high)
                             : (val >  low && val <  high);
        if (hit) result.push_back(i);
    }
    return result;
}

class QueryEngine : public IQueryEngine {
public:
    std::vector<std::size_t> range_search(
        const IDataStore& store,
        const RangeQuery& query) const override;
};

} // namespace mini1

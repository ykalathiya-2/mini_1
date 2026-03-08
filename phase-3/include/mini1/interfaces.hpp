#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace mini1 {

struct LoadSummary {
    std::size_t total_rows   = 0;
    std::size_t valid_rows   = 0;
    std::size_t invalid_rows = 0;
};

struct ColumnRange {
    std::string column;
    double      low       = 0.0;
    double      high      = 0.0;
    bool        inclusive  = true;
};

struct RangeQuery {
    std::vector<ColumnRange> predicates;  // AND of all ranges

    // Convenience: single-column constructor for backward compat.
    RangeQuery() = default;
    RangeQuery(const std::string& col, double lo, double hi, bool incl)
        : predicates{{col, lo, hi, incl}} {}
};

} // namespace mini1

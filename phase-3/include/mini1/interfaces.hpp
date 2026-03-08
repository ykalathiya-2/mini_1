#pragma once

#include <cstddef>
#include <string>

namespace mini1 {

struct LoadSummary {
    std::size_t total_rows   = 0;
    std::size_t valid_rows   = 0;
    std::size_t invalid_rows = 0;
};

struct RangeQuery {
    std::string column;
    double      low       = 0.0;
    double      high      = 0.0;
    bool        inclusive  = true;
};

} // namespace mini1

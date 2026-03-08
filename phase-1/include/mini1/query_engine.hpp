#pragma once

#include "mini1/interfaces.hpp"

#include <vector>

namespace mini1 {

// Resolve a column name to the corresponding field value on a TaxiTrip.
// Returns false if the column is unknown.
bool resolve_column(const TaxiTrip& row, const std::string& col, double& out);

class QueryEngine : public IQueryEngine {
public:
    std::vector<std::size_t> range_search(
        const IDataStore& store,
        const RangeQuery& query) const override;
};

} // namespace mini1

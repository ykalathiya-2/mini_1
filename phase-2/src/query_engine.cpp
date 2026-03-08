#include "mini1/query_engine.hpp"

namespace mini1 {

bool resolve_column(const TaxiTrip& row, const std::string& col, double& out) {
    if (col == "vendor_id"      || col == "VendorID")      { out = row.vendor_id;             return true; }
    if (col == "passenger_count")                           { out = row.passenger_count;       return true; }
    if (col == "trip_distance")                             { out = row.trip_distance;         return true; }
    if (col == "ratecode_id"    || col == "RatecodeID")     { out = row.ratecode_id;           return true; }
    if (col == "pu_location_id" || col == "PULocationID")   { out = row.pu_location_id;        return true; }
    if (col == "do_location_id" || col == "DOLocationID")   { out = row.do_location_id;        return true; }
    if (col == "payment_type")                              { out = row.payment_type;          return true; }
    if (col == "fare_amount")                               { out = row.fare_amount;           return true; }
    if (col == "extra")                                     { out = row.extra;                 return true; }
    if (col == "mta_tax")                                   { out = row.mta_tax;               return true; }
    if (col == "tip_amount")                                { out = row.tip_amount;            return true; }
    if (col == "tolls_amount")                              { out = row.tolls_amount;          return true; }
    if (col == "improvement_surcharge")                     { out = row.improvement_surcharge; return true; }
    if (col == "total_amount")                              { out = row.total_amount;          return true; }
    return false;
}

std::vector<std::size_t> QueryEngine::range_search(
        const IDataStore& store,
        const RangeQuery& query) const
{
    const std::size_t n = store.row_count();
    std::vector<std::size_t> result;
    result.reserve(n / 100);

    for (std::size_t i = 0; i < n; ++i) {
        const auto& row = store.row_at(i);
        if (!row.valid) continue;

        bool match = true;
        for (const auto& pred : query.predicates) {
            double val = 0;
            if (!resolve_column(row, pred.column, val)) { match = false; break; }
            bool hit = pred.inclusive ? (val >= pred.low && val <= pred.high)
                                     : (val >  pred.low && val <  pred.high);
            if (!hit) { match = false; break; }
        }
        if (match) result.push_back(i);
    }
    return result;
}

} // namespace mini1

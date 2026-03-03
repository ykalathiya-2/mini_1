#include "mini1/query_engine.hpp"

#include <stdexcept>

namespace mini1 {

std::vector<std::size_t> QueryEngine::range_search(
        const IDataStore& store,
        const RangeQuery& query) const
{
    const auto& col = query.column;
    auto lo = query.low, hi = query.high;
    bool inc = query.inclusive;

    // Each branch instantiates range_scan with a different accessor lambda.
    if (col == "vendor_id" || col == "VendorID")
        return range_scan(store, [](const TaxiTrip& r){ return r.vendor_id; }, lo, hi, inc);
    if (col == "passenger_count")
        return range_scan(store, [](const TaxiTrip& r){ return r.passenger_count; }, lo, hi, inc);
    if (col == "trip_distance")
        return range_scan(store, [](const TaxiTrip& r){ return r.trip_distance; }, lo, hi, inc);
    if (col == "RatecodeID" || col == "ratecode_id")
        return range_scan(store, [](const TaxiTrip& r){ return r.ratecode_id; }, lo, hi, inc);
    if (col == "PULocationID" || col == "pu_location_id")
        return range_scan(store, [](const TaxiTrip& r){ return r.pu_location_id; }, lo, hi, inc);
    if (col == "DOLocationID" || col == "do_location_id")
        return range_scan(store, [](const TaxiTrip& r){ return r.do_location_id; }, lo, hi, inc);
    if (col == "payment_type")
        return range_scan(store, [](const TaxiTrip& r){ return r.payment_type; }, lo, hi, inc);
    if (col == "fare_amount")
        return range_scan(store, [](const TaxiTrip& r){ return r.fare_amount; }, lo, hi, inc);
    if (col == "extra")
        return range_scan(store, [](const TaxiTrip& r){ return r.extra; }, lo, hi, inc);
    if (col == "mta_tax")
        return range_scan(store, [](const TaxiTrip& r){ return r.mta_tax; }, lo, hi, inc);
    if (col == "tip_amount")
        return range_scan(store, [](const TaxiTrip& r){ return r.tip_amount; }, lo, hi, inc);
    if (col == "tolls_amount")
        return range_scan(store, [](const TaxiTrip& r){ return r.tolls_amount; }, lo, hi, inc);
    if (col == "improvement_surcharge")
        return range_scan(store, [](const TaxiTrip& r){ return r.improvement_surcharge; }, lo, hi, inc);
    if (col == "total_amount")
        return range_scan(store, [](const TaxiTrip& r){ return r.total_amount; }, lo, hi, inc);

    return {}; // unknown column
}

} // namespace mini1

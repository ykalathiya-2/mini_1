#include "mini1/parallel_query_engine.hpp"
#include "mini1/taxi_trip.hpp"

#include <atomic>
#include <cstring>
#include <omp.h>

namespace mini1 {

ParallelQueryEngine::ParallelQueryEngine(int threads)
    : threads_(threads) {}

template <typename Accessor>
static std::vector<std::size_t> par_scan(
        const std::vector<TaxiTrip>& rows,
        Accessor acc,
        double low, double high, bool inclusive,
        int nt) {

    const std::size_t n = rows.size();

    // Shared output: pre-sized for worst case.
    // Threads batch hits into a small stack buffer, then claim a contiguous
    // range in the shared array with a single fetch_add. This keeps atomic
    // contention to O(n / BATCH) instead of O(n) per thread.
    std::vector<std::size_t> result(n);
    std::atomic<std::size_t> tail{0};

    constexpr std::size_t BATCH = 256;

    #pragma omp parallel num_threads(nt)
    {
        std::size_t buf[BATCH];
        std::size_t cnt = 0;

        auto flush = [&]() {
            std::size_t base = tail.fetch_add(cnt, std::memory_order_relaxed);
            std::memcpy(&result[base], buf, cnt * sizeof(std::size_t));
            cnt = 0;
        };

        #pragma omp for schedule(static)
        for (std::size_t i = 0; i < n; ++i) {
            const TaxiTrip& row = rows[i];
            if (!row.valid) continue;
            double v = acc(row);
            bool hit = inclusive ? (v >= low && v <= high)
                                 : (v >  low && v <  high);
            if (hit) {
                buf[cnt++] = i;
                if (cnt == BATCH) flush();
            }
        }
        if (cnt > 0) flush();
    }

    result.resize(tail.load(std::memory_order_relaxed));
    return result;
}

std::vector<std::size_t> ParallelQueryEngine::range_search(
        const IDataStore& store,
        const RangeQuery& query) const {

    const auto& rows     = store.rows();
    const std::string& col = query.column;
    double low             = query.low;
    double high            = query.high;
    bool   inclusive       = query.inclusive;
    int    nt              = (threads_ == 0) ? omp_get_max_threads() : threads_;

    if (col == "vendor_id" || col == "VendorID")
        return par_scan(rows, [](const TaxiTrip& r){ return static_cast<double>(r.vendor_id); },        low, high, inclusive, nt);
    if (col == "passenger_count")
        return par_scan(rows, [](const TaxiTrip& r){ return static_cast<double>(r.passenger_count); },  low, high, inclusive, nt);
    if (col == "trip_distance")
        return par_scan(rows, [](const TaxiTrip& r){ return r.trip_distance; },                         low, high, inclusive, nt);
    if (col == "ratecode_id" || col == "RatecodeID")
        return par_scan(rows, [](const TaxiTrip& r){ return static_cast<double>(r.ratecode_id); },      low, high, inclusive, nt);
    if (col == "pu_location_id" || col == "PULocationID")
        return par_scan(rows, [](const TaxiTrip& r){ return static_cast<double>(r.pu_location_id); },   low, high, inclusive, nt);
    if (col == "do_location_id" || col == "DOLocationID")
        return par_scan(rows, [](const TaxiTrip& r){ return static_cast<double>(r.do_location_id); },   low, high, inclusive, nt);
    if (col == "payment_type")
        return par_scan(rows, [](const TaxiTrip& r){ return static_cast<double>(r.payment_type); },     low, high, inclusive, nt);
    if (col == "fare_amount")
        return par_scan(rows, [](const TaxiTrip& r){ return r.fare_amount; },                           low, high, inclusive, nt);
    if (col == "extra")
        return par_scan(rows, [](const TaxiTrip& r){ return r.extra; },                                 low, high, inclusive, nt);
    if (col == "mta_tax")
        return par_scan(rows, [](const TaxiTrip& r){ return r.mta_tax; },                               low, high, inclusive, nt);
    if (col == "tip_amount")
        return par_scan(rows, [](const TaxiTrip& r){ return r.tip_amount; },                            low, high, inclusive, nt);
    if (col == "tolls_amount")
        return par_scan(rows, [](const TaxiTrip& r){ return r.tolls_amount; },                          low, high, inclusive, nt);
    if (col == "improvement_surcharge")
        return par_scan(rows, [](const TaxiTrip& r){ return r.improvement_surcharge; },                 low, high, inclusive, nt);
    if (col == "total_amount")
        return par_scan(rows, [](const TaxiTrip& r){ return r.total_amount; },                          low, high, inclusive, nt);

    return {};
}

} // namespace mini1

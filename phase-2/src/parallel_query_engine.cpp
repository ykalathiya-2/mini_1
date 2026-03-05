#include "mini1/parallel_query_engine.hpp"
#include "mini1/taxi_trip.hpp"

#include <functional>
#include <omp.h>

namespace mini1 {

ParallelQueryEngine::ParallelQueryEngine(int threads)
    : threads_(threads) {}

std::vector<std::size_t> ParallelQueryEngine::range_search(
        const IDataStore& store,
        const RangeQuery& query) const {

    const auto& rows  = store.rows();
    const std::size_t n = rows.size();

    std::function<double(const TaxiTrip&)> acc;
    const std::string& col = query.column;

    if      (col == "vendor_id"          || col == "VendorID")
        acc = [](const TaxiTrip& r){ return static_cast<double>(r.vendor_id); };
    else if (col == "passenger_count")
        acc = [](const TaxiTrip& r){ return static_cast<double>(r.passenger_count); };
    else if (col == "trip_distance")
        acc = [](const TaxiTrip& r){ return r.trip_distance; };
    else if (col == "ratecode_id"        || col == "RatecodeID")
        acc = [](const TaxiTrip& r){ return static_cast<double>(r.ratecode_id); };
    else if (col == "pu_location_id"     || col == "PULocationID")
        acc = [](const TaxiTrip& r){ return static_cast<double>(r.pu_location_id); };
    else if (col == "do_location_id"     || col == "DOLocationID")
        acc = [](const TaxiTrip& r){ return static_cast<double>(r.do_location_id); };
    else if (col == "payment_type")
        acc = [](const TaxiTrip& r){ return static_cast<double>(r.payment_type); };
    else if (col == "fare_amount")
        acc = [](const TaxiTrip& r){ return r.fare_amount; };
    else if (col == "extra")
        acc = [](const TaxiTrip& r){ return r.extra; };
    else if (col == "mta_tax")
        acc = [](const TaxiTrip& r){ return r.mta_tax; };
    else if (col == "tip_amount")
        acc = [](const TaxiTrip& r){ return r.tip_amount; };
    else if (col == "tolls_amount")
        acc = [](const TaxiTrip& r){ return r.tolls_amount; };
    else if (col == "improvement_surcharge")
        acc = [](const TaxiTrip& r){ return r.improvement_surcharge; };
    else if (col == "total_amount")
        acc = [](const TaxiTrip& r){ return r.total_amount; };
    else
        return {};

    double low       = query.low;
    double high      = query.high;
    bool   inclusive = query.inclusive;

    int nt = (threads_ == 0) ? omp_get_max_threads() : threads_;

    std::vector<std::vector<std::size_t>> partial(nt);

    #pragma omp parallel num_threads(nt)
    {
        int tid = omp_get_thread_num();
        auto& local = partial[tid];
        local.reserve(n / (static_cast<std::size_t>(nt) * 100));

        #pragma omp for schedule(static)
        for (std::size_t i = 0; i < n; ++i) {
            const TaxiTrip& row = rows[i];
            if (!row.valid) continue;
            double v = acc(row);
            bool hit = inclusive ? (v >= low && v <= high)
                                 : (v >  low && v <  high);
            if (hit) local.push_back(i);
        }
    }

    std::vector<std::size_t> result;
    result.reserve(n / 100);
    for (auto& p : partial)
        result.insert(result.end(), p.begin(), p.end());
    return result;
}

} // namespace mini1

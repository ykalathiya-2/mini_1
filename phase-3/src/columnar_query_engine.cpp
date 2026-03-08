#include "mini1/columnar_query_engine.hpp"

#include <atomic>
#include <cstring>
#include <omp.h>

namespace mini1 {

ColumnarQueryEngine::ColumnarQueryEngine(int threads) : threads_(threads) {}

// Scan a single contiguous column array + valid mask.
// Only touches sizeof(T)*n + n bytes of memory — for double columns on 20M
// rows that is 180 MB vs 4+ GB for an AoS full-struct scan.
template <typename T>
static std::vector<std::size_t> col_scan(
        const T* col, const uint8_t* valid, std::size_t n,
        double low, double high, bool inclusive,
        int nt) {

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
            if (!valid[i]) continue;
            double v = static_cast<double>(col[i]);
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

std::vector<std::size_t> ColumnarQueryEngine::range_search(
        const ColumnarStore& store,
        const RangeQuery& query) const {

    const std::string& col = query.column;
    double low   = query.low;
    double high  = query.high;
    bool   incl  = query.inclusive;
    int    nt    = (threads_ == 0) ? omp_get_max_threads() : threads_;
    std::size_t n = store.size();
    const uint8_t* v = store.valid.data();

    if (col == "vendor_id" || col == "VendorID")
        return col_scan(store.vendor_id.data(), v, n, low, high, incl, nt);
    if (col == "passenger_count")
        return col_scan(store.passenger_count.data(), v, n, low, high, incl, nt);
    if (col == "trip_distance")
        return col_scan(store.trip_distance.data(), v, n, low, high, incl, nt);
    if (col == "ratecode_id" || col == "RatecodeID")
        return col_scan(store.ratecode_id.data(), v, n, low, high, incl, nt);
    if (col == "pu_location_id" || col == "PULocationID")
        return col_scan(store.pu_location_id.data(), v, n, low, high, incl, nt);
    if (col == "do_location_id" || col == "DOLocationID")
        return col_scan(store.do_location_id.data(), v, n, low, high, incl, nt);
    if (col == "payment_type")
        return col_scan(store.payment_type.data(), v, n, low, high, incl, nt);
    if (col == "fare_amount")
        return col_scan(store.fare_amount.data(), v, n, low, high, incl, nt);
    if (col == "extra")
        return col_scan(store.extra.data(), v, n, low, high, incl, nt);
    if (col == "mta_tax")
        return col_scan(store.mta_tax.data(), v, n, low, high, incl, nt);
    if (col == "tip_amount")
        return col_scan(store.tip_amount.data(), v, n, low, high, incl, nt);
    if (col == "tolls_amount")
        return col_scan(store.tolls_amount.data(), v, n, low, high, incl, nt);
    if (col == "improvement_surcharge")
        return col_scan(store.improvement_surcharge.data(), v, n, low, high, incl, nt);
    if (col == "total_amount")
        return col_scan(store.total_amount.data(), v, n, low, high, incl, nt);

    return {};
}

} // namespace mini1

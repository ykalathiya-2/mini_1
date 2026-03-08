#include "mini1/columnar_query_engine.hpp"

#include <atomic>
#include <cstring>
#include <omp.h>

namespace mini1 {

ColumnarQueryEngine::ColumnarQueryEngine(int threads) : threads_(threads) {}

// Resolve a column name to a (base pointer, element size, is_double) triple.
// int32 columns are cast to double during comparison.
struct ColRef {
    const void* data = nullptr;
    bool is_double   = false;
};

static ColRef resolve_col(const ColumnarStore& store, const std::string& col) {
    if (col == "vendor_id"      || col == "VendorID")      return {store.vendor_id.data(),             false};
    if (col == "passenger_count")                           return {store.passenger_count.data(),       false};
    if (col == "trip_distance")                             return {store.trip_distance.data(),         true};
    if (col == "ratecode_id"    || col == "RatecodeID")     return {store.ratecode_id.data(),           false};
    if (col == "pu_location_id" || col == "PULocationID")   return {store.pu_location_id.data(),        false};
    if (col == "do_location_id" || col == "DOLocationID")   return {store.do_location_id.data(),        false};
    if (col == "payment_type")                              return {store.payment_type.data(),          false};
    if (col == "fare_amount")                               return {store.fare_amount.data(),           true};
    if (col == "extra")                                     return {store.extra.data(),                 true};
    if (col == "mta_tax")                                   return {store.mta_tax.data(),               true};
    if (col == "tip_amount")                                return {store.tip_amount.data(),            true};
    if (col == "tolls_amount")                              return {store.tolls_amount.data(),          true};
    if (col == "improvement_surcharge")                     return {store.improvement_surcharge.data(), true};
    if (col == "total_amount")                              return {store.total_amount.data(),          true};
    return {};
}

static inline double read_val(const ColRef& ref, std::size_t i) {
    if (ref.is_double)
        return static_cast<const double*>(ref.data)[i];
    return static_cast<double>(static_cast<const int32_t*>(ref.data)[i]);
}

std::vector<std::size_t> ColumnarQueryEngine::range_search(
        const ColumnarStore& store,
        const RangeQuery& query) const {

    int nt = (threads_ == 0) ? omp_get_max_threads() : threads_;
    std::size_t n = store.size();
    const uint8_t* valid = store.valid.data();

    // Pre-resolve all column pointers so inner loop has no string comparisons.
    struct ResolvedPred {
        ColRef col;
        double low;
        double high;
        bool   inclusive;
    };
    std::vector<ResolvedPred> preds;
    preds.reserve(query.predicates.size());
    for (const auto& p : query.predicates) {
        ColRef cr = resolve_col(store, p.column);
        if (!cr.data) return {};  // unknown column
        preds.push_back({cr, p.low, p.high, p.inclusive});
    }

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

            bool match = true;
            for (const auto& pred : preds) {
                double v = read_val(pred.col, i);
                bool hit = pred.inclusive ? (v >= pred.low && v <= pred.high)
                                         : (v >  pred.low && v <  pred.high);
                if (!hit) { match = false; break; }
            }
            if (match) {
                buf[cnt++] = i;
                if (cnt == BATCH) flush();
            }
        }
        if (cnt > 0) flush();
    }

    result.resize(tail.load(std::memory_order_relaxed));
    return result;
}

} // namespace mini1

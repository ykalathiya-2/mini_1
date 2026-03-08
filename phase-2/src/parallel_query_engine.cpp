#include "mini1/parallel_query_engine.hpp"
#include "mini1/query_engine.hpp"

#include <atomic>
#include <cstring>
#include <omp.h>

namespace mini1 {

ParallelQueryEngine::ParallelQueryEngine(int threads)
    : threads_(threads) {}

std::vector<std::size_t> ParallelQueryEngine::range_search(
        const IDataStore& store,
        const RangeQuery& query) const {

    const auto& rows = store.rows();
    const std::size_t n = rows.size();
    int nt = (threads_ == 0) ? omp_get_max_threads() : threads_;

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

            bool match = true;
            for (const auto& pred : query.predicates) {
                double val = 0;
                if (!resolve_column(row, pred.column, val)) { match = false; break; }
                bool hit = pred.inclusive ? (val >= pred.low && val <= pred.high)
                                         : (val >  pred.low && val <  pred.high);
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

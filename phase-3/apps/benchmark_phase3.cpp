#include "mini1/columnar_store.hpp"
#include "mini1/columnar_csv_reader.hpp"
#include "mini1/columnar_query_engine.hpp"

#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <omp.h>
#include <sstream>
#include <sys/resource.h>

#if defined(__APPLE__)
#  include <mach/mach.h>
#endif

using namespace mini1;
using Clock = std::chrono::steady_clock;

static uint64_t footprint_bytes() {
#if defined(__APPLE__)
    task_vm_info_data_t info{};
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS)
        return info.phys_footprint;
#endif
    return 0;
}

static RangeQuery parse_predicates(const std::string& spec) {
    RangeQuery q;
    std::istringstream ss(spec);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        auto p1 = tok.find(':');
        auto p2 = tok.find(':', p1 + 1);
        if (p1 == std::string::npos || p2 == std::string::npos) continue;
        ColumnRange cr;
        cr.column = tok.substr(0, p1);
        cr.low    = std::stod(tok.substr(p1 + 1, p2 - p1 - 1));
        cr.high   = std::stod(tok.substr(p2 + 1));
        q.predicates.push_back(cr);
    }
    return q;
}

static std::string predicate_desc(const RangeQuery& q) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < q.predicates.size(); ++i) {
        if (i) oss << " AND ";
        const auto& p = q.predicates[i];
        oss << p.column << " in [" << p.low << ", " << p.high << "]";
    }
    return oss.str();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: benchmark_phase3 <csv_path> <predicates> [reps] [threads]\n"
                  << "  predicates: col:lo:hi[,col:lo:hi,...]\n"
                  << "  OR old form: col lo hi\n"
                  << "  threads=0  use all available cores (default)\n";
        return 1;
    }

    std::string csv_path = argv[1];
    int reps    = 10;
    int threads = 0;
    RangeQuery query;

    if (argc > 2 && std::strchr(argv[2], ':') != nullptr) {
        query = parse_predicates(argv[2]);
        if (argc > 3) reps    = std::stoi(argv[3]);
        if (argc > 4) threads = std::stoi(argv[4]);
    } else if (argc > 4) {
        std::string column = argv[2];
        double lo = std::stod(argv[3]);
        double hi = std::stod(argv[4]);
        query = RangeQuery{column, lo, hi, true};
        if (argc > 5) reps    = std::stoi(argv[5]);
        if (argc > 6) threads = std::stoi(argv[6]);
    } else {
        query = RangeQuery{"trip_distance", 1.0, 3.0, true};
    }

    int actual_threads = threads;
    if (actual_threads == 0) {
        #pragma omp parallel
        { actual_threads = omp_get_num_threads(); }
    }

    auto w = std::setw(44);
    std::cout << "\n=== Phase 3 benchmark (SoA) ===\n";
    std::cout << w << "dataset: " << csv_path << "\n";
    std::cout << w << "query: " << predicate_desc(query) << "\n";
    std::cout << w << "reps: " << reps << "\n";
    std::cout << w << "threads: " << actual_threads << "\n";

    // ---- Initial load (to report row counts and load time) ----
    ColumnarCsvReader reader(threads);
    ColumnarStore store;
    LoadSummary summary{};
    auto t0 = Clock::now();
    reader.read_columnar(csv_path, store, summary);
    double load_ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
    uint64_t fp = footprint_bytes();

    std::cout << "\n" << w << "total rows: " << summary.total_rows << "\n";
    std::cout << w << "valid rows: " << summary.valid_rows << "\n";
    std::cout << w << "invalid rows: " << summary.invalid_rows << "\n";

    std::cout << std::fixed;
    std::cout << "\n--- Load ---\n";
    std::cout << std::setprecision(1);
    std::cout << w << "SoA parallel load wall ms: " << load_ms << "\n";
    std::cout << w << "footprint GB: " << fp / 1e9 << "\n";

    // ---- Query (hot: data already loaded, query only) ----
    double query_ms = 0;
    std::size_t hits = 0;
    {
        ColumnarQueryEngine engine(threads);
        auto tq = Clock::now();
        for (int i = 0; i < reps; ++i)
            hits = engine.range_search(store, query).size();
        query_ms = std::chrono::duration<double, std::milli>(Clock::now() - tq).count() / reps;
    }

    std::cout << std::setprecision(1);
    std::cout << "\n--- Query (hot, avg over " << reps << " reps) ---\n";
    std::cout << w << actual_threads << "-thread SoA wall ms: "
              << query_ms << "  hits=" << hits << "\n";

    std::cout << "\n";
    return 0;
}

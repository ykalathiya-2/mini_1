#include "mini1/columnar_store.hpp"
#include "mini1/columnar_csv_reader.hpp"
#include "mini1/columnar_query_engine.hpp"

#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <omp.h>
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

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: benchmark_phase3 <csv_path> [column] [low] [high]"
                     " [reps] [threads]\n"
                  << "  threads=0  use all available cores (default)\n";
        return 1;
    }

    std::string csv_path = argv[1];
    std::string column   = (argc > 2) ? argv[2] : "trip_distance";
    double  lo           = (argc > 3) ? std::stod(argv[3]) : 1.0;
    double  hi           = (argc > 4) ? std::stod(argv[4]) : 3.0;
    int     reps         = (argc > 5) ? std::stoi(argv[5]) : 10;
    int     threads      = (argc > 6) ? std::stoi(argv[6]) : 0;

    int actual_threads = threads;
    if (actual_threads == 0) {
        #pragma omp parallel
        { actual_threads = omp_get_num_threads(); }
    }

    RangeQuery query{column, lo, hi, true};

    auto w = std::setw(44);
    std::cout << "\n=== Phase 3 benchmark (SoA) ===\n";
    std::cout << w << "dataset: " << csv_path << "\n";
    std::cout << w << "query: " << column << " in [" << lo << ", " << hi << "]\n";
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

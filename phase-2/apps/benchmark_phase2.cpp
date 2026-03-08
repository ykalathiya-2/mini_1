#include "mini1/parallel_csv_reader.hpp"
#include "mini1/row_store.hpp"
#include "mini1/parallel_query_engine.hpp"

#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <omp.h>
#include <sstream>
#include <string>
#include <sys/resource.h>

#if defined(__APPLE__)
#  include <mach/mach.h>
#endif

using namespace mini1;
using Clock = std::chrono::steady_clock;

struct Snap {
    double   user_ms   = 0;
    double   sys_ms    = 0;
    uint64_t footprint = 0;
};

static double tv_ms(const timeval& tv) {
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static Snap snap() {
    Snap s;
    rusage ru{};
    getrusage(RUSAGE_SELF, &ru);
    s.user_ms = tv_ms(ru.ru_utime);
    s.sys_ms  = tv_ms(ru.ru_stime);
#if defined(__APPLE__)
    task_vm_info_data_t info{};
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS)
        s.footprint = info.phys_footprint;
#endif
    return s;
}

struct Metrics {
    double   wall_ms      = 0;
    double   cpu_user_ms  = 0;
    double   cpu_sys_ms   = 0;
    uint64_t footprint    = 0;
};

static Metrics diff(const Snap& before, const Snap& after, double wall_ms) {
    Metrics m;
    m.wall_ms     = wall_ms;
    m.cpu_user_ms = after.user_ms  - before.user_ms;
    m.cpu_sys_ms  = after.sys_ms   - before.sys_ms;
    m.footprint   = after.footprint;
    return m;
}

static void print_report(
        const std::string& csv_path,
        const std::string& column,
        double lo, double hi,
        int reps, int threads,
        const LoadSummary& summary,
        const Metrics& load_p,
        const Metrics& qparallel,
        std::size_t hits_p) {

    auto w = std::setw(28);
    std::cout << "\n=== Phase 2 benchmark (parallel) ===\n";
    std::cout << w << "dataset: "      << csv_path << "\n";
    std::cout << w << "query: "        << column << " in [" << lo << ", " << hi << "]\n";
    std::cout << w << "reps: "         << reps << "\n";
    std::cout << w << "threads: "      << threads << "\n";
    std::cout << w << "total rows: "   << summary.total_rows   << "\n";
    std::cout << w << "valid rows: "   << summary.valid_rows   << "\n";
    std::cout << w << "invalid rows: " << summary.invalid_rows << "\n";

    std::cout << "\n--- Load ---\n";
    std::cout << std::fixed << std::setprecision(1);
    std::cout << w << "parallel wall ms: " << load_p.wall_ms   << "\n";
    std::cout << w << "cpu user ms: "      << load_p.cpu_user_ms << "\n";
    std::cout << w << "cpu sys ms: "       << load_p.cpu_sys_ms  << "\n";
    std::cout << w << "footprint GB: "     << load_p.footprint / 1e9 << "\n";

    std::cout << "\n--- Query (avg over " << reps << " reps) ---\n";
    std::cout << std::setprecision(1);
    std::cout << w << "parallel wall ms: " << qparallel.wall_ms << "  hits=" << hits_p << "\n";
    std::cout << std::setprecision(2);
    std::cout << w << "cpu total ms: "     << (qparallel.cpu_user_ms + qparallel.cpu_sys_ms) << "\n";
    std::cout << "\n";
}

static std::string csv_line(
        const std::string& csv_path,
        const std::string& column,
        double lo, double hi,
        int reps, int threads,
        const LoadSummary& summary,
        const Metrics& load_p,
        const Metrics& qparallel,
        std::size_t hits_p) {

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    oss << csv_path   << ","
        << column     << ","
        << lo         << ","
        << hi         << ","
        << reps       << ","
        << threads    << ","
        << summary.total_rows   << ","
        << summary.valid_rows   << ","
        << summary.invalid_rows << ","
        << load_p.wall_ms       << ","
        << qparallel.wall_ms    << ","
        << hits_p               << ","
        << load_p.cpu_user_ms   << ","
        << load_p.cpu_sys_ms    << ","
        << load_p.footprint     << ","
        << (qparallel.cpu_user_ms + qparallel.cpu_sys_ms);
    return oss.str();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: benchmark_phase2 <csv_path> [column] [low] [high]"
                     " [reps] [threads] [--csv]\n"
                  << "  threads=0  use all available cores (default)\n";
        return 1;
    }

    std::string csv_path = argv[1];
    std::string column   = (argc > 2) ? argv[2] : "trip_distance";
    double  lo           = (argc > 3) ? std::stod(argv[3]) : 1.0;
    double  hi           = (argc > 4) ? std::stod(argv[4]) : 3.0;
    int     reps         = (argc > 5) ? std::stoi(argv[5]) : 10;
    int     threads      = (argc > 6) ? std::stoi(argv[6]) : 0;
    bool    csv_mode     = false;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--csv") == 0) csv_mode = true;

    RangeQuery query{column, lo, hi, true};

    // Parallel load.
    Metrics load_p;
    LoadSummary summary;
    RowStore store;
    {
        ParallelCsvReader reader(threads);
        std::vector<TaxiTrip> rows;
        auto before = snap();
        auto t0     = Clock::now();
        reader.read(csv_path, rows, summary);
        double wall = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
        load_p = diff(before, snap(), wall);
        store.load(std::move(rows));
    }

    // Parallel query (hot: data already loaded).
    ParallelQueryEngine parallel_engine(threads);
    std::size_t hits_p = 0;
    Metrics qparallel;
    {
        auto before = snap();
        auto t0     = Clock::now();
        for (int i = 0; i < reps; ++i)
            hits_p = parallel_engine.range_search(store, query).size();
        double wall = std::chrono::duration<double, std::milli>(Clock::now() - t0).count() / reps;
        qparallel = diff(before, snap(), wall);
        qparallel.cpu_user_ms /= reps;
        qparallel.cpu_sys_ms  /= reps;
    }

    int actual_threads = threads;
    if (actual_threads == 0) {
        #pragma omp parallel
        { actual_threads = omp_get_num_threads(); }
    }

    if (csv_mode)
        std::cout << csv_line(csv_path, column, lo, hi, reps, actual_threads,
                               summary, load_p, qparallel, hits_p) << "\n";
    else
        print_report(csv_path, column, lo, hi, reps, actual_threads,
                     summary, load_p, qparallel, hits_p);

    return 0;
}

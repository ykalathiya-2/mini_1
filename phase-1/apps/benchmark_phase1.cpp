#include "mini1/data_facade.hpp"
#include "mini1/interfaces.hpp"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/resource.h>
#include <vector>

#if defined(__APPLE__)
#include <mach/mach.h>
#endif

namespace {

struct UsageSnapshot {
    double   user_ms  = 0.0;
    double   sys_ms   = 0.0;
    uint64_t rss      = 0;
    uint64_t footprint = 0;
};

double tv_to_ms(const timeval& tv) {
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

UsageSnapshot snap() {
    rusage ru{};
    if (getrusage(RUSAGE_SELF, &ru) != 0) return {};
    UsageSnapshot s;
    s.user_ms = tv_to_ms(ru.ru_utime);
    s.sys_ms  = tv_to_ms(ru.ru_stime);
#if defined(__APPLE__)
    s.rss = static_cast<uint64_t>(ru.ru_maxrss); // bytes on macOS
#else
    s.rss = static_cast<uint64_t>(ru.ru_maxrss) * 1024; // KB on Linux
#endif
#if defined(__APPLE__)
    task_vm_info_data_t vm{};
    mach_msg_type_number_t cnt = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO,
                  reinterpret_cast<task_info_t>(&vm), &cnt) == KERN_SUCCESS)
        s.footprint = vm.phys_footprint;
#endif
    return s;
}

struct PhaseMetrics {
    double   elapsed_ms = 0;
    double   cpu_user   = 0;
    double   cpu_sys    = 0;
    double   cpu_total  = 0;
    uint64_t peak_rss   = 0;
    uint64_t footprint  = 0;
};

PhaseMetrics measure(const UsageSnapshot& a, const UsageSnapshot& b, double elapsed) {
    PhaseMetrics m;
    m.elapsed_ms = elapsed;
    m.cpu_user   = b.user_ms - a.user_ms;
    m.cpu_sys    = b.sys_ms  - a.sys_ms;
    m.cpu_total  = m.cpu_user + m.cpu_sys;
    m.peak_rss   = b.rss;
    m.footprint  = b.footprint;
    return m;
}

void print_report(const mini1::LoadSummary& sum,
                  const std::string& col, double lo, double hi,
                  int reps, std::size_t hits,
                  const PhaseMetrics& load, const PhaseMetrics& query,
                  uint64_t overall_rss)
{
    double avg_q = reps > 0 ? query.elapsed_ms / reps : 0.0;
    std::cout << std::fixed << std::setprecision(3)
        << "Rows total:           " << sum.total_rows      << "\n"
        << "Rows valid:           " << sum.valid_rows      << "\n"
        << "Rows invalid:         " << sum.invalid_rows    << "\n"
        << "Load time ms:         " << load.elapsed_ms     << "\n"
        << "Query total ms:       " << query.elapsed_ms    << "\n"
        << "Avg query ms:         " << avg_q               << "\n"
        << "Load CPU user ms:     " << load.cpu_user       << "\n"
        << "Load CPU sys ms:      " << load.cpu_sys        << "\n"
        << "Load CPU total ms:    " << load.cpu_total      << "\n"
        << "Query CPU user ms:    " << query.cpu_user      << "\n"
        << "Query CPU sys ms:     " << query.cpu_sys       << "\n"
        << "Query CPU total ms:   " << query.cpu_total     << "\n"
        << "Load peak RSS bytes:  " << load.peak_rss       << "\n"
        << "Load footprint bytes: " << load.footprint      << "\n"
        << "Query peak RSS bytes: " << query.peak_rss      << "\n"
        << "Query footprint bytes:" << query.footprint     << "\n"
        << "Overall peak RSS:     " << overall_rss         << "\n"
        << "Query column:         " << col                 << "\n"
        << "Range:                [" << lo << ", " << hi << "]\n"
        << "Repeats:              " << reps                << "\n"
        << "Last hits:            " << hits                << "\n";
}

std::string csv_line(const mini1::LoadSummary& sum,
                     std::size_t hits, int reps,
                     const PhaseMetrics& load, const PhaseMetrics& query,
                     uint64_t overall_rss)
{
    double avg_q = reps > 0 ? query.elapsed_ms / reps : 0.0;
    std::ostringstream o;
    o << std::fixed << std::setprecision(3)
      << load.elapsed_ms    << ',' << query.elapsed_ms << ',' << avg_q     << ','
      << hits               << ',' << sum.total_rows   << ',' << sum.valid_rows << ','
      << sum.invalid_rows   << ','
      << load.cpu_user      << ',' << load.cpu_sys     << ',' << load.cpu_total << ','
      << query.cpu_user     << ',' << query.cpu_sys    << ',' << query.cpu_total << ','
      << load.peak_rss      << ',' << load.footprint   << ','
      << query.peak_rss     << ',' << query.footprint  << ','
      << overall_rss;
    return o.str();
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: benchmark_phase1 <csv_path> [column low high repeats] [--csv]\n";
        return 1;
    }

    bool csv_mode = false;
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--csv") csv_mode = true;
        else              args.push_back(a);
    }

    if (args.empty()) { std::cerr << "Missing csv_path.\n"; return 1; }

    std::string csv_path = args[0];
    std::string query_col = "trip_distance";
    double lo = 1.0, hi = 3.0;
    int reps = 10;

    if (args.size() >= 5) {
        query_col = args[1];
        lo   = std::stod(args[2]);
        hi   = std::stod(args[3]);
        reps = std::stoi(args[4]);
    }
    if (reps < 1) reps = 1;

    mini1::DataFacade data;
    auto u0 = snap();
    auto t0 = std::chrono::steady_clock::now();
    if (!data.load(csv_path)) {
        std::cerr << "Failed to load: " << csv_path << "\n";
        return 2;
    }
    auto t1 = std::chrono::steady_clock::now();
    auto u1 = snap();
    double load_ms = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() / 1000.0;
    auto load_m = measure(u0, u1, load_ms);
    auto summary = data.load_summary();

    mini1::RangeQuery q{query_col, lo, hi, true};
    auto u2 = snap();
    auto t2 = std::chrono::steady_clock::now();
    std::size_t hits = 0;
    for (int i = 0; i < reps; ++i)
        hits = data.range_search(q).size();
    auto t3 = std::chrono::steady_clock::now();
    auto u3 = snap();
    double query_ms = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count() / 1000.0;
    auto query_m = measure(u2, u3, query_ms);

    if (csv_mode)
        std::cout << csv_line(summary, hits, reps, load_m, query_m, u3.rss) << "\n";
    else
        print_report(summary, query_col, lo, hi, reps, hits, load_m, query_m, u3.rss);

    return 0;
}

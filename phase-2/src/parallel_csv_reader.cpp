#include "mini1/parallel_csv_reader.hpp"

#include <fstream>
#include <omp.h>

namespace mini1 {

ParallelCsvReader::ParallelCsvReader(int threads)
    : threads_(threads) {}

bool ParallelCsvReader::read(const std::string& source_path,
                              std::vector<TaxiTrip>& rows_out,
                              LoadSummary& summary_out) {
    // Find where the data starts (byte offset just after the header line).
    std::ifstream probe(source_path);
    if (!probe) return false;
    std::string header;
    if (!std::getline(probe, header)) return false;
    std::streamoff data_start = probe.tellg();
    probe.seekg(0, std::ios::end);
    std::streamoff file_size = probe.tellg();
    probe.close();

    if (data_start < 0 || file_size <= data_start) return false;

    int nt = (threads_ == 0) ? omp_get_max_threads() : threads_;
    std::streamoff data_size = file_size - data_start;

    std::vector<std::vector<TaxiTrip>> partial_rows(nt);
    std::vector<std::size_t> partial_valid(nt, 0);
    std::vector<std::size_t> partial_invalid(nt, 0);

    #pragma omp parallel num_threads(nt)
    {
        int tid = omp_get_thread_num();

        std::streamoff chunk_start = data_start + (data_size * tid / nt);
        std::streamoff chunk_end   = data_start + (data_size * (tid + 1) / nt);

        std::ifstream stream(source_path);
        stream.seekg(chunk_start);

        if (tid != 0) {
            stream.seekg(chunk_start - 1);
            char prev = '\0';
            stream.get(prev);
            if (prev != '\n') {
                std::string skip;
                std::getline(stream, skip);
            }
        }

        std::streamoff pos = stream.tellg();
        std::string line;
        while (std::getline(stream, line)) {
            pos += static_cast<std::streamoff>(line.size()) + 1;

            auto fields = parse_csv_line(line);
            TaxiTrip row = parse_row(fields);

            if (row.valid) ++partial_valid[tid];
            else           ++partial_invalid[tid];

            partial_rows[tid].push_back(std::move(row));

            if (tid < nt - 1 && pos >= chunk_end)
                break;
        }
    }

    std::size_t total = 0;
    for (auto& p : partial_rows) total += p.size();
    rows_out.clear();
    rows_out.reserve(total);
    for (auto& p : partial_rows)
        for (auto& r : p)
            rows_out.push_back(std::move(r));

    summary_out.total_rows   = total;
    summary_out.valid_rows   = 0;
    summary_out.invalid_rows = 0;
    for (int i = 0; i < nt; ++i) {
        summary_out.valid_rows   += partial_valid[i];
        summary_out.invalid_rows += partial_invalid[i];
    }
    return true;
}

} // namespace mini1

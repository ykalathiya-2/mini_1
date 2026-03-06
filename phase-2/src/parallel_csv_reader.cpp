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

    // Pass 1: each thread reads raw lines from its byte chunk.
    std::vector<std::vector<std::string>> partial_lines(nt);
    std::vector<std::size_t> counts(nt, 0);

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
            partial_lines[tid].push_back(std::move(line));
            if (tid < nt - 1 && pos >= chunk_end)
                break;
        }
        counts[tid] = partial_lines[tid].size();
    }

    // Prefix sum to get each thread's write offset into the shared array.
    std::vector<std::size_t> offsets(nt + 1, 0);
    for (int i = 0; i < nt; ++i)
        offsets[i + 1] = offsets[i] + counts[i];

    std::size_t total = offsets[nt];
    rows_out.clear();
    rows_out.resize(total);

    std::vector<std::size_t> partial_valid(nt, 0);
    std::vector<std::size_t> partial_invalid(nt, 0);

    // Pass 2: each thread parses its lines and writes directly into its slice.
    #pragma omp parallel num_threads(nt)
    {
        int tid = omp_get_thread_num();
        std::size_t idx = offsets[tid];

        for (auto& raw : partial_lines[tid]) {
            auto fields  = parse_csv_line(raw);
            TaxiTrip row = parse_row(fields);

            if (row.valid) ++partial_valid[tid];
            else           ++partial_invalid[tid];

            rows_out[idx++] = std::move(row);
        }

        // Free line buffer as soon as this thread is done with it.
        std::vector<std::string>().swap(partial_lines[tid]);
    }

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

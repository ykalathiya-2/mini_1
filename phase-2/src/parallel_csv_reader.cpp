#include "mini1/parallel_csv_reader.hpp"

#include <cstring>
#include <fcntl.h>
#include <omp.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace mini1 {

ParallelCsvReader::ParallelCsvReader(int threads)
    : threads_(threads) {}

bool ParallelCsvReader::read(const std::string& source_path,
                              std::vector<TaxiTrip>& rows_out,
                              LoadSummary& summary_out) {
    int fd = open(source_path.c_str(), O_RDONLY);
    if (fd < 0) return false;

    struct stat st{};
    if (fstat(fd, &st) != 0) { close(fd); return false; }
    auto file_size = static_cast<std::size_t>(st.st_size);
    if (file_size == 0) { close(fd); return false; }

    const char* mapped = static_cast<const char*>(
        mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0));
    close(fd);
    if (mapped == MAP_FAILED) return false;

#ifdef MADV_SEQUENTIAL
    madvise(const_cast<char*>(mapped), file_size, MADV_SEQUENTIAL);
#endif

    const char* file_end = mapped + file_size;

    // Skip header line.
    const char* header_end = static_cast<const char*>(
        std::memchr(mapped, '\n', file_size));
    if (!header_end) { munmap(const_cast<char*>(mapped), file_size); return false; }

    const char* data_start = header_end + 1;
    std::size_t data_size = static_cast<std::size_t>(file_end - data_start);

    int nt = (threads_ == 0) ? omp_get_max_threads() : threads_;

    // Pass 1: count newlines per chunk using memchr on mmap'd memory.
    std::vector<std::size_t> counts(nt, 0);

    #pragma omp parallel num_threads(nt)
    {
        int tid = omp_get_thread_num();
        const char* chunk_begin = data_start + (data_size * tid / nt);
        const char* chunk_end   = data_start + (data_size * (tid + 1) / nt);

        std::size_t nl = 0;
        const char* p = chunk_begin;
        while (p < chunk_end) {
            const char* found = static_cast<const char*>(
                std::memchr(p, '\n', static_cast<std::size_t>(chunk_end - p)));
            if (!found) break;
            ++nl;
            p = found + 1;
        }
        counts[tid] = nl;
    }

    // Prefix sum for per-thread write offsets.
    std::vector<std::size_t> offsets(nt + 1, 0);
    for (int i = 0; i < nt; ++i)
        offsets[i + 1] = offsets[i] + counts[i];

    std::size_t total = offsets[nt];
    rows_out.clear();
    rows_out.resize(total);

    std::vector<std::size_t> partial_valid(nt, 0);
    std::vector<std::size_t> partial_invalid(nt, 0);

    // Pass 2: each thread parses its chunk directly from mmap'd memory.
    #pragma omp parallel num_threads(nt)
    {
        int tid = omp_get_thread_num();
        const char* chunk_begin = data_start + (data_size * tid / nt);
        const char* chunk_end   = data_start + (data_size * (tid + 1) / nt);

        // Align to line boundary (except thread 0).
        if (tid != 0) {
            const char* nl = static_cast<const char*>(
                std::memchr(chunk_begin, '\n', static_cast<std::size_t>(file_end - chunk_begin)));
            if (nl) chunk_begin = nl + 1;
            else    chunk_begin = file_end;
        }

        // Find the true end for last partial line in this chunk.
        if (tid < nt - 1) {
            const char* nl = static_cast<const char*>(
                std::memchr(chunk_end, '\n', static_cast<std::size_t>(file_end - chunk_end)));
            if (nl) chunk_end = nl + 1;
            else    chunk_end = file_end;
        } else {
            chunk_end = file_end;
        }

        std::size_t idx = offsets[tid];
        const char* pos = chunk_begin;

        while (pos < chunk_end && idx < total) {
            const char* nl = static_cast<const char*>(
                std::memchr(pos, '\n', static_cast<std::size_t>(chunk_end - pos)));
            const char* line_end = nl ? nl : chunk_end;

            if (line_end == pos) { pos = line_end + 1; continue; }

            const char* row_end = line_end;
            if (row_end > pos && *(row_end - 1) == '\r') --row_end;

            parse_row_into(pos, row_end, rows_out[idx]);

            if (rows_out[idx].valid) ++partial_valid[tid];
            else                     ++partial_invalid[tid];

            ++idx;
            pos = line_end + 1;
        }
    }

    munmap(const_cast<char*>(mapped), file_size);

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

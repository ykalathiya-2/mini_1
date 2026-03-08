#include "mini1/columnar_csv_reader.hpp"

#include <cstring>
#include <fcntl.h>
#include <omp.h>
#include <string_view>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace mini1 {

ColumnarCsvReader::ColumnarCsvReader(int threads) : threads_(threads) {}

bool ColumnarCsvReader::read_columnar(const std::string& source_path,
                                       ColumnarStore& store,
                                       LoadSummary& summary) {
    // mmap the entire file — zero-copy, no per-row allocation.
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
    std::size_t data_size  = static_cast<std::size_t>(file_end - data_start);

    int nt = (threads_ == 0) ? omp_get_max_threads() : threads_;

    // Pass 1: count newlines per chunk using memchr (SIMD-accelerated).
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
    store.resize(total);

    std::vector<std::size_t> partial_valid(nt, 0);
    std::vector<std::size_t> partial_invalid(nt, 0);

    // Pass 2: parse directly from mmap'd memory into columnar arrays.
    // All string_views point into the mmap — zero heap allocation per row.
    #pragma omp parallel num_threads(nt)
    {
        int tid = omp_get_thread_num();
        const char* chunk_begin = data_start + (data_size * tid / nt);
        const char* chunk_end   = data_start + (data_size * (tid + 1) / nt);

        // Align to line boundary (skip partial line at start, except thread 0).
        const char* pos = chunk_begin;
        if (tid != 0 && pos > data_start && *(pos - 1) != '\n') {
            const char* nl = static_cast<const char*>(
                std::memchr(pos, '\n', static_cast<std::size_t>(file_end - pos)));
            pos = nl ? nl + 1 : file_end;
        }

        std::size_t idx = offsets[tid];

        while (pos < file_end) {
            const char* nl = static_cast<const char*>(
                std::memchr(pos, '\n', static_cast<std::size_t>(file_end - pos)));
            const char* line_end = nl ? nl : file_end;

            // Skip empty lines (e.g. trailing newline).
            if (line_end == pos) {
                pos = line_end + 1;
                if (tid < nt - 1 && pos >= chunk_end) break;
                continue;
            }

            // Handle \r\n.
            const char* row_end = line_end;
            if (row_end > pos && *(row_end - 1) == '\r') --row_end;

            const char* p = pos;
            const char* e = row_end;
            auto field = [&]() -> std::string_view {
                return CsvParse::strip_quotes(CsvParse::next_field(p, e));
            };

            bool ok = true;
            ok = ok && CsvParse::try_parse_int32_sv(field(),  store.vendor_id[idx]);          // 0
            { auto sv = field(); store.pickup_datetime[idx]  = std::string(CsvParse::trim_sv(sv)); }  // 1
            { auto sv = field(); store.dropoff_datetime[idx] = std::string(CsvParse::trim_sv(sv)); }  // 2
            ok = ok && CsvParse::try_parse_int32_sv(field(),  store.passenger_count[idx]);     // 3
            ok = ok && CsvParse::try_parse_double_sv(field(), store.trip_distance[idx]);       // 4
            ok = ok && CsvParse::try_parse_int32_sv(field(),  store.ratecode_id[idx]);         // 5
            { auto sv = field(); store.store_and_fwd_flag[idx] = std::string(CsvParse::trim_sv(sv)); } // 6
            ok = ok && CsvParse::try_parse_int32_sv(field(),  store.pu_location_id[idx]);      // 7
            ok = ok && CsvParse::try_parse_int32_sv(field(),  store.do_location_id[idx]);      // 8
            ok = ok && CsvParse::try_parse_int32_sv(field(),  store.payment_type[idx]);        // 9
            ok = ok && CsvParse::try_parse_double_sv(field(), store.fare_amount[idx]);         // 10
            ok = ok && CsvParse::try_parse_double_sv(field(), store.extra[idx]);               // 11
            ok = ok && CsvParse::try_parse_double_sv(field(), store.mta_tax[idx]);             // 12
            ok = ok && CsvParse::try_parse_double_sv(field(), store.tip_amount[idx]);          // 13
            ok = ok && CsvParse::try_parse_double_sv(field(), store.tolls_amount[idx]);        // 14
            ok = ok && CsvParse::try_parse_double_sv(field(), store.improvement_surcharge[idx]); // 15
            ok = ok && CsvParse::try_parse_double_sv(field(), store.total_amount[idx]);        // 16

            store.valid[idx] = ok ? 1 : 0;
            if (ok) ++partial_valid[tid];
            else    ++partial_invalid[tid];

            ++idx;

            pos = line_end + 1;
            if (tid < nt - 1 && pos >= chunk_end)
                break;
        }
    }

    munmap(const_cast<char*>(mapped), file_size);

    summary.total_rows   = total;
    summary.valid_rows   = 0;
    summary.invalid_rows = 0;
    for (int i = 0; i < nt; ++i) {
        summary.valid_rows   += partial_valid[i];
        summary.invalid_rows += partial_invalid[i];
    }
    return true;
}

} // namespace mini1

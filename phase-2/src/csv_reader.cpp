#include "mini1/csv_reader.hpp"

#include <charconv>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <string_view>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace mini1 {

std::string_view CsvReader::trim_sv(std::string_view sv) {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front())))
        sv.remove_prefix(1);
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back())))
        sv.remove_suffix(1);
    return sv;
}

bool CsvReader::try_parse_int32_sv(std::string_view sv, int32_t& value) {
    sv = trim_sv(sv);
    if (sv.empty()) return false;
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value, 10);
    return ec == std::errc{} && ptr == sv.data() + sv.size();
}

bool CsvReader::try_parse_double_sv(std::string_view sv, double& value) {
    sv = trim_sv(sv);
    if (sv.empty()) return false;
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
    return ec == std::errc{} && std::isfinite(value);
}

std::string_view CsvReader::next_field(const char*& pos, const char* end) {
    const char* start = pos;
    if (start >= end || *start != '"') {
        const char* comma = static_cast<const char*>(std::memchr(pos, ',', static_cast<std::size_t>(end - pos)));
        if (comma) { pos = comma + 1; return {start, static_cast<std::size_t>(comma - start)}; }
        pos = end;
        return {start, static_cast<std::size_t>(end - start)};
    }
    bool in_quotes = false;
    while (pos < end) {
        char c = *pos;
        if (c == '"') {
            in_quotes = !in_quotes;
            ++pos;
        } else if (c == ',' && !in_quotes) {
            std::string_view result(start, static_cast<std::size_t>(pos - start));
            ++pos;
            return result;
        } else {
            ++pos;
        }
    }
    return {start, static_cast<std::size_t>(pos - start)};
}

std::string_view CsvReader::strip_quotes(std::string_view sv) {
    if (sv.size() >= 2 && sv.front() == '"' && sv.back() == '"') {
        sv.remove_prefix(1);
        sv.remove_suffix(1);
    }
    return sv;
}

void CsvReader::parse_row_into(const char* pos, const char* end, TaxiTrip& row) {
    auto field = [&]() -> std::string_view {
        return strip_quotes(next_field(pos, end));
    };

    bool ok = true;

    ok = ok && try_parse_int32_sv(field(), row.vendor_id);                  // 0
    row.pickup_datetime  = std::string(trim_sv(field()));                    // 1
    row.dropoff_datetime = std::string(trim_sv(field()));                    // 2
    ok = ok && try_parse_int32_sv(field(), row.passenger_count);            // 3
    ok = ok && try_parse_double_sv(field(), row.trip_distance);             // 4
    ok = ok && try_parse_int32_sv(field(), row.ratecode_id);                // 5
    row.store_and_fwd_flag = std::string(trim_sv(field()));                  // 6
    ok = ok && try_parse_int32_sv(field(), row.pu_location_id);             // 7
    ok = ok && try_parse_int32_sv(field(), row.do_location_id);             // 8
    ok = ok && try_parse_int32_sv(field(), row.payment_type);               // 9
    ok = ok && try_parse_double_sv(field(), row.fare_amount);               // 10
    ok = ok && try_parse_double_sv(field(), row.extra);                     // 11
    ok = ok && try_parse_double_sv(field(), row.mta_tax);                   // 12
    ok = ok && try_parse_double_sv(field(), row.tip_amount);                // 13
    ok = ok && try_parse_double_sv(field(), row.tolls_amount);              // 14
    ok = ok && try_parse_double_sv(field(), row.improvement_surcharge);     // 15
    ok = ok && try_parse_double_sv(field(), row.total_amount);              // 16

    row.valid = ok;
}

bool CsvReader::read(const std::string& source_path,
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

    const char* header_end = static_cast<const char*>(
        std::memchr(mapped, '\n', file_size));
    if (!header_end) { munmap(const_cast<char*>(mapped), file_size); return false; }

    const char* data_start = header_end + 1;

    std::size_t total = 0;
    {
        const char* p = data_start;
        while (p < file_end) {
            const char* found = static_cast<const char*>(
                std::memchr(p, '\n', static_cast<std::size_t>(file_end - p)));
            if (!found) break;
            ++total;
            p = found + 1;
        }
    }

    rows_out.clear();
    rows_out.resize(total);
    summary_out = {};
    summary_out.total_rows = total;

    const char* pos = data_start;
    std::size_t idx = 0;

    while (pos < file_end && idx < total) {
        const char* nl = static_cast<const char*>(
            std::memchr(pos, '\n', static_cast<std::size_t>(file_end - pos)));
        const char* line_end = nl ? nl : file_end;

        if (line_end == pos) { pos = line_end + 1; continue; }

        const char* row_end = line_end;
        if (row_end > pos && *(row_end - 1) == '\r') --row_end;

        parse_row_into(pos, row_end, rows_out[idx]);

        if (rows_out[idx].valid) ++summary_out.valid_rows;
        else                     ++summary_out.invalid_rows;

        ++idx;
        pos = line_end + 1;
    }

    munmap(const_cast<char*>(mapped), file_size);
    return true;
}

} // namespace mini1

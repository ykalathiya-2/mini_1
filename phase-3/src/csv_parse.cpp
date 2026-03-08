#include "mini1/csv_parse.hpp"

#include <charconv>
#include <cmath>
#include <cstring>

namespace mini1 {

std::string_view CsvParse::trim_sv(std::string_view sv) {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front())))
        sv.remove_prefix(1);
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back())))
        sv.remove_suffix(1);
    return sv;
}

bool CsvParse::try_parse_int32_sv(std::string_view sv, int32_t& value) {
    sv = trim_sv(sv);
    if (sv.empty()) return false;
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value, 10);
    return ec == std::errc{} && ptr == sv.data() + sv.size();
}

bool CsvParse::try_parse_double_sv(std::string_view sv, double& value) {
    sv = trim_sv(sv);
    if (sv.empty()) return false;
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
    return ec == std::errc{} && std::isfinite(value);
}

std::string_view CsvParse::next_field(const char*& pos, const char* end) {
    const char* start = pos;
    // Fast path: unquoted field — use vectorized memchr
    if (start >= end || *start != '"') {
        const char* comma = static_cast<const char*>(std::memchr(pos, ',', static_cast<std::size_t>(end - pos)));
        if (comma) { pos = comma + 1; return {start, static_cast<std::size_t>(comma - start)}; }
        pos = end;
        return {start, static_cast<std::size_t>(end - start)};
    }
    // Slow path: quoted field
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

std::string_view CsvParse::strip_quotes(std::string_view sv) {
    if (sv.size() >= 2 && sv.front() == '"' && sv.back() == '"') {
        sv.remove_prefix(1);
        sv.remove_suffix(1);
    }
    return sv;
}

} // namespace mini1

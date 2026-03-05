#include "mini1/csv_reader.hpp"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <fstream>

namespace mini1 {

std::string CsvReader::trim(const std::string& text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])))
        ++begin;

    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])))
        --end;

    return text.substr(begin, end - begin);
}

std::vector<std::string> CsvReader::parse_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    current.reserve(64);
    bool in_quotes = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '"') {
            if (in_quotes && i + 1 < line.size() && line[i + 1] == '"') {
                current.push_back('"');
                ++i;
            } else {
                in_quotes = !in_quotes;
            }
            continue;
        }
        if (c == ',' && !in_quotes) {
            fields.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    fields.push_back(current);
    return fields;
}

bool CsvReader::try_parse_int32(const std::string& text, int32_t& value) {
    auto trimmed = trim(text);
    if (trimmed.empty()) return false;
    auto [ptr, ec] = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), value, 10);
    return ec == std::errc{} && ptr == trimmed.data() + trimmed.size();
}

bool CsvReader::try_parse_double(const std::string& text, double& value) {
    auto trimmed = trim(text);
    if (trimmed.empty()) return false;
    char* end = nullptr;
    value = std::strtod(trimmed.c_str(), &end);
    return end != trimmed.c_str() && *end == '\0' && std::isfinite(value);
}

TaxiTrip CsvReader::parse_row(const std::vector<std::string>& fields) {
    TaxiTrip row;
    if (fields.size() < 17) {
        row.valid = false;
        return row;
    }

    bool ok = true;
    ok = ok && try_parse_int32(fields[0],  row.vendor_id);
    row.pickup_datetime  = trim(fields[1]);
    row.dropoff_datetime = trim(fields[2]);
    ok = ok && try_parse_int32(fields[3],  row.passenger_count);
    ok = ok && try_parse_double(fields[4], row.trip_distance);
    ok = ok && try_parse_int32(fields[5],  row.ratecode_id);
    row.store_and_fwd_flag = trim(fields[6]);
    ok = ok && try_parse_int32(fields[7],  row.pu_location_id);
    ok = ok && try_parse_int32(fields[8],  row.do_location_id);
    ok = ok && try_parse_int32(fields[9],  row.payment_type);
    ok = ok && try_parse_double(fields[10], row.fare_amount);
    ok = ok && try_parse_double(fields[11], row.extra);
    ok = ok && try_parse_double(fields[12], row.mta_tax);
    ok = ok && try_parse_double(fields[13], row.tip_amount);
    ok = ok && try_parse_double(fields[14], row.tolls_amount);
    ok = ok && try_parse_double(fields[15], row.improvement_surcharge);
    ok = ok && try_parse_double(fields[16], row.total_amount);

    row.valid = ok;
    return row;
}

bool CsvReader::read(const std::string& source_path,
                     std::vector<TaxiTrip>& rows_out,
                     LoadSummary& summary_out) {
    std::ifstream input(source_path);
    if (!input) return false;

    rows_out.clear();
    summary_out = {};

    std::string line;
    if (!std::getline(input, line)) return false; // skip header

    while (std::getline(input, line)) {
        auto fields = parse_csv_line(line);
        TaxiTrip row = parse_row(fields);

        ++summary_out.total_rows;
        if (row.valid) ++summary_out.valid_rows;
        else           ++summary_out.invalid_rows;

        rows_out.push_back(std::move(row));
    }
    return true;
}

} // namespace mini1

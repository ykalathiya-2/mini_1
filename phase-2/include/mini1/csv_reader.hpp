#pragma once

#include "mini1/interfaces.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace mini1 {

class CsvReader : public IDataReader {
public:
    bool read(const std::string& source_path,
              std::vector<TaxiTrip>& rows_out,
              LoadSummary& summary_out) override;

protected:
    static std::string_view next_field(const char*& pos, const char* end);
    static std::string_view strip_quotes(std::string_view sv);
    static std::string_view trim_sv(std::string_view sv);
    static bool try_parse_int32_sv(std::string_view sv, int32_t& value);
    static bool try_parse_double_sv(std::string_view sv, double& value);
    static void parse_row_into(const char* pos, const char* end, TaxiTrip& row);
};

} // namespace mini1

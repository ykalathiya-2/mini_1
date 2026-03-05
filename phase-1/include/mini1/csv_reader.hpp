#pragma once

#include "mini1/interfaces.hpp"

#include <string>
#include <vector>

namespace mini1 {

class CsvReader : public IDataReader {
public:
    bool read(const std::string& source_path,
              std::vector<TaxiTrip>& rows_out,
              LoadSummary& summary_out) override;

protected:
    static std::vector<std::string> parse_csv_line(const std::string& line);
    static std::string              trim(const std::string& text);
    static bool  try_parse_int32(const std::string& text, int32_t& value);
    static bool  try_parse_double(const std::string& text, double& value);
    static TaxiTrip parse_row(const std::vector<std::string>& fields);
};

} // namespace mini1

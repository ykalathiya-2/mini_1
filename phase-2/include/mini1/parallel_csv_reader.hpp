#pragma once

#include "mini1/csv_reader.hpp"

namespace mini1 {

class ParallelCsvReader : public CsvReader {
public:
    explicit ParallelCsvReader(int threads = 0);

    bool read(const std::string& source_path,
              std::vector<TaxiTrip>& rows_out,
              LoadSummary& summary_out) override;

private:
    int threads_;
};

} // namespace mini1

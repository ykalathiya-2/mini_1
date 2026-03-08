#pragma once

#include "mini1/columnar_store.hpp"
#include "mini1/csv_parse.hpp"
#include "mini1/interfaces.hpp"

#include <string>

namespace mini1 {

class ColumnarCsvReader {
public:
    explicit ColumnarCsvReader(int threads = 0);

    bool read_columnar(const std::string& source_path,
                       ColumnarStore& store,
                       LoadSummary& summary);

private:
    int threads_;
};

} // namespace mini1

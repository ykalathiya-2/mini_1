#pragma once

#include "mini1/interfaces.hpp"

#include <memory>
#include <string>
#include <vector>

namespace mini1 {

// Facade — wires together reader, store, and query engine behind one API.
class DataFacade {
public:
    DataFacade();
    DataFacade(std::unique_ptr<IDataReader> reader,
               std::unique_ptr<IDataStore>  store,
               std::unique_ptr<IQueryEngine> engine);
    ~DataFacade();

    bool load(const std::string& csv_path);

    std::size_t              row_count() const;
    const TaxiTrip&          row_at(std::size_t index) const;
    LoadSummary              load_summary() const;
    std::vector<std::size_t> range_search(const RangeQuery& query) const;

private:
    std::unique_ptr<IDataReader>  reader_;
    std::unique_ptr<IDataStore>   store_;
    std::unique_ptr<IQueryEngine> engine_;
    LoadSummary                   summary_;
};

} // namespace mini1

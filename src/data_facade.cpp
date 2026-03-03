#include "mini1/data_facade.hpp"
#include "mini1/csv_reader.hpp"
#include "mini1/query_engine.hpp"
#include "mini1/row_store.hpp"

#include <utility>

namespace mini1 {

DataFacade::DataFacade()
    : reader_(std::make_unique<CsvReader>()),
      store_(std::make_unique<RowStore>()),
      engine_(std::make_unique<QueryEngine>()) {}

DataFacade::DataFacade(std::unique_ptr<IDataReader> reader,
                       std::unique_ptr<IDataStore>  store,
                       std::unique_ptr<IQueryEngine> engine)
    : reader_(std::move(reader)),
      store_(std::move(store)),
      engine_(std::move(engine)) {}

DataFacade::~DataFacade() = default;

bool DataFacade::load(const std::string& csv_path) {
    summary_ = {};
    std::vector<TaxiTrip> rows;
    if (!reader_->read(csv_path, rows, summary_))
        return false;
    store_->load(std::move(rows));
    return true;
}

std::size_t DataFacade::row_count() const { return store_->row_count(); }

const TaxiTrip& DataFacade::row_at(std::size_t index) const { return store_->row_at(index); }

LoadSummary DataFacade::load_summary() const { return summary_; }

std::vector<std::size_t> DataFacade::range_search(const RangeQuery& query) const {
    return engine_->range_search(*store_, query);
}

} // namespace mini1

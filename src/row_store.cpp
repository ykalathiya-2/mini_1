#include "mini1/row_store.hpp"

#include <stdexcept>
#include <utility>

namespace mini1 {

void RowStore::load(std::vector<TaxiTrip>&& rows) { rows_ = std::move(rows); }

std::size_t RowStore::row_count() const { return rows_.size(); }

const TaxiTrip& RowStore::row_at(std::size_t index) const {
    if (index >= rows_.size())
        throw std::out_of_range("RowStore::row_at: index out of range");
    return rows_[index];
}

const std::vector<TaxiTrip>& RowStore::rows() const { return rows_; }

} // namespace mini1

#pragma once

#include "mini1/interfaces.hpp"

#include <vector>

namespace mini1 {

class RowStore : public IDataStore {
public:
    void                         load(std::vector<TaxiTrip>&& rows) override;
    std::size_t                  row_count() const override;
    const TaxiTrip&              row_at(std::size_t index) const override;
    const std::vector<TaxiTrip>& rows() const override;

private:
    std::vector<TaxiTrip> rows_;
};

} // namespace mini1

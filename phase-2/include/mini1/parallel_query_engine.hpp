#pragma once

#include "mini1/interfaces.hpp"

namespace mini1 {

class ParallelQueryEngine : public IQueryEngine {
public:
    explicit ParallelQueryEngine(int threads = 0);

    std::vector<std::size_t> range_search(const IDataStore& store,
                                          const RangeQuery& query) const override;

private:
    int threads_;
};

} // namespace mini1

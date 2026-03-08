#pragma once

#include "mini1/columnar_store.hpp"
#include "mini1/interfaces.hpp"

#include <cstddef>
#include <vector>

namespace mini1 {

class ColumnarQueryEngine {
public:
    explicit ColumnarQueryEngine(int threads = 0);

    std::vector<std::size_t> range_search(
            const ColumnarStore& store,
            const RangeQuery& query) const;

private:
    int threads_;
};

} // namespace mini1

#pragma once

#include "mini1/taxi_trip.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace mini1 {

struct LoadSummary {
    std::size_t total_rows   = 0;
    std::size_t valid_rows   = 0;
    std::size_t invalid_rows = 0;
};

struct RangeQuery {
    std::string column;
    double      low       = 0.0;
    double      high      = 0.0;
    bool        inclusive  = true;
};

// Abstract interfaces

class IDataReader {
public:
    virtual ~IDataReader() = default;
    virtual bool read(const std::string& source_path,
                      std::vector<TaxiTrip>& rows_out,
                      LoadSummary& summary_out) = 0;
};

class IDataStore {
public:
    virtual ~IDataStore() = default;
    virtual void                         load(std::vector<TaxiTrip>&& rows) = 0;
    virtual std::size_t                  row_count() const = 0;
    virtual const TaxiTrip&              row_at(std::size_t index) const = 0;
    virtual const std::vector<TaxiTrip>& rows() const = 0;
};

class IQueryEngine {
public:
    virtual ~IQueryEngine() = default;
    virtual std::vector<std::size_t> range_search(
        const IDataStore& store,
        const RangeQuery& query) const = 0;
};

} // namespace mini1

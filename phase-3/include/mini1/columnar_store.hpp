#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mini1 {

// Columnar (SoA) layout — all 17 fields stored.
struct ColumnarStore {
    std::vector<int32_t> vendor_id;
    std::vector<std::string> pickup_datetime;
    std::vector<std::string> dropoff_datetime;
    std::vector<int32_t> passenger_count;
    std::vector<double>  trip_distance;
    std::vector<int32_t> ratecode_id;
    std::vector<int32_t> pu_location_id;
    std::vector<int32_t> do_location_id;
    std::vector<std::string> store_and_fwd_flag;
    std::vector<int32_t> payment_type;
    std::vector<double>  fare_amount;
    std::vector<double>  extra;
    std::vector<double>  mta_tax;
    std::vector<double>  tip_amount;
    std::vector<double>  tolls_amount;
    std::vector<double>  improvement_surcharge;
    std::vector<double>  total_amount;
    std::vector<uint8_t> valid;

    std::size_t size() const { return valid.size(); }
    void resize(std::size_t n);
};

} // namespace mini1

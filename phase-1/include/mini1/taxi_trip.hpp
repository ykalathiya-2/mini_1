#pragma once

#include <cstdint>
#include <string>

namespace mini1 {

struct TaxiTrip {
    int32_t     vendor_id             = 0;
    std::string pickup_datetime;
    std::string dropoff_datetime;
    int32_t     passenger_count       = 0;
    double      trip_distance         = 0.0;
    int32_t     ratecode_id           = 0;
    std::string store_and_fwd_flag;
    int32_t     pu_location_id        = 0;
    int32_t     do_location_id        = 0;
    int32_t     payment_type          = 0;
    double      fare_amount           = 0.0;
    double      extra                 = 0.0;
    double      mta_tax               = 0.0;
    double      tip_amount            = 0.0;
    double      tolls_amount          = 0.0;
    double      improvement_surcharge = 0.0;
    double      total_amount          = 0.0;
    bool        valid                 = false;
};

} // namespace mini1

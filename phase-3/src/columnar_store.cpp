#include "mini1/columnar_store.hpp"

namespace mini1 {

void ColumnarStore::resize(std::size_t n) {
    vendor_id.resize(n);
    pickup_datetime.resize(n);
    dropoff_datetime.resize(n);
    passenger_count.resize(n);
    trip_distance.resize(n);
    ratecode_id.resize(n);
    pu_location_id.resize(n);
    do_location_id.resize(n);
    store_and_fwd_flag.resize(n);
    payment_type.resize(n);
    fare_amount.resize(n);
    extra.resize(n);
    mta_tax.resize(n);
    tip_amount.resize(n);
    tolls_amount.resize(n);
    improvement_surcharge.resize(n);
    total_amount.resize(n);
    valid.resize(n, 0);
}

} // namespace mini1

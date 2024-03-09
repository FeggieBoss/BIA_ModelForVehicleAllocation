#ifndef DEFINE_DATA_H
#define DEFINE_DATA_H

#include "main.h"
#include "params.h"
#include "trucks.h"
#include "orders.h"
#include "distances.h"

class Data {
private:
    unsigned int min_timestamp;
public:
    Params params;
    Trucks trucks;
    Orders orders;    
    Distances dists;

    Data() = default;
    Data(
        const std::string &params_path,
        const std::string &trucks_path,
        const std::string &orders_path,
        const std::string &dists_path
    );

    void ShiftTimestamps();
    std::optional<double> MoveBetweenOrders(const Order& previous, const Order& current);
    double GetRealOrderRevenue(size_t ind);
};


#endif // DEFINE_DATA_H
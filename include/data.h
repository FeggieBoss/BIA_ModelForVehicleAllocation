#ifndef DEFINE_DATA_H
#define DEFINE_DATA_H

#include "main.h"
#include "params.h"
#include "trucks.h"
#include "orders.h"
#include "distances.h"

#include <unordered_map>

class Data {
public:
    // Note: being initialized in ShiftTimestamps
    unsigned int min_timestamp;

    // Note: being initialized in SqueezeCitiesIds
    std::unordered_map<unsigned int, unsigned int> id_to_real_city;
    size_t cities_count;

    Params params;
    Trucks trucks;
    Orders orders;    
    Distances dists;

    Data() = default;
    // Note: call ShiftTimestamps(), SqueezeCitiesIds()
    Data(
        const std::string &params_path,
        const std::string &trucks_path,
        const std::string &orders_path,
        const std::string &dists_path
    );
    Data(const Data& other);

    void ShiftTimestamps();
    void SqueezeCitiesIds();

    /*
        Checks if 'truck' can move complete 'previous' order and move to 'current' order
        if so returns revenue of doing so
        !!!Note!!!: checks masks compatibility only for 'current' order
    */
    std::optional<double> MoveBetweenOrders(const Truck& truck, const Order& previous, const Order& current) const;
    // same as above but without checking masks compatibility
    std::optional<double> MoveBetweenOrders(const Order& previous, const Order& current) const;
    
    std::optional<double> CostMovingBetweenOrders(const Order& previous, const Order& current) const;
    double GetRealOrderRevenue(const Order& order) const;
    double GetRealOrderRevenue(size_t ind) const;
    double GetFreeMovementCost(double distance) const;
    double GetWaitingCost(double mins) const;

private:
};


#endif // DEFINE_DATA_H
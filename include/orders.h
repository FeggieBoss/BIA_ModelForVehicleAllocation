#ifndef DEFINE_ORDERS_H
#define DEFINE_ORDERS_H

#include "main.h"
#include "xlsx_cell.h"

#include <OpenXLSX.hpp>

#include <string>
#include <vector>

class Order {
public:
    unsigned int order_id;
    bool obligation;
    unsigned int start_time;
    unsigned int finish_time;
    unsigned int from_city;
    unsigned int to_city;
    int mask_load_type = 0;
    int mask_trailer_type = 0;
    double distance;
    double revenue;
public:
    Order(){};
    Order(
        unsigned int order_id,
        bool obligation,
        unsigned int start_time,
        unsigned int finish_time,
        unsigned int from_city,
        unsigned int to_city,
        const std::string &str_load_type, 
        const std::string &str_trailer_type,
        double distance,
        double revenue
    );
    Order(
        unsigned int order_id,
        bool obligation,
        unsigned int start_time,
        unsigned int finish_time,
        unsigned int from_city,
        unsigned int to_city,
        int mask_load_type, 
        int mask_trailer_type,
        double distance,
        double revenue
    );
    
    #ifdef DEBUG_MODE
    void DebugPrint() const;
    #endif
};

class Orders {
private:
    std::vector<Order> orders_;
public:
    Orders(){};
    Orders(const std::string &path_to_xlsx);
    Orders(const std::vector<Order>& orders);
    Orders(const Orders& other);

    std::vector<Order>::iterator begin() {  // NOLINT
        return orders_.begin();
    }

    std::vector<Order>::iterator end() {  // NOLINT
        return orders_.end();
    }

    Order GetOrder(size_t ind) const;
    const Order& GetOrderConst(size_t ind) const;
    void AddOrder(const Order& order);
    size_t Size() const;

    #ifdef DEBUG_MODE
    void DebugPrint() const;
    #endif
};

#endif // DEFINE_ORDERS_H
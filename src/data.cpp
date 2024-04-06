#include "data.h"

Data::Data(
    const std::string &params_path,
    const std::string &trucks_path,
    const std::string &orders_path,
    const std::string &dists_path
):  params(params_path),
    trucks(trucks_path),
    orders(orders_path),
    dists(dists_path) {}

void Data::ShiftTimestamps() {
    min_timestamp = UINT32_MAX;
    for(auto &truck : trucks) {
        min_timestamp = std::min(min_timestamp, truck.init_time);
    }
    for(auto &order : orders) {
        min_timestamp = std::min(min_timestamp, order.start_time);
    }

    for(auto &truck : trucks) {
        truck.init_time -= min_timestamp;
    }
    for(auto &order : orders) {
        order.start_time -= min_timestamp;
        order.finish_time -= min_timestamp;
    }
}


std::optional<double> Data::MoveBetweenOrders(const Order& previous, const Order& current) const {
    double cost = 0.;

    auto dist_between_orders = dists.GetDistance(previous.to_city, current.from_city); 
    if (!dist_between_orders.has_value())
        return std::nullopt;

    double d = dist_between_orders.value();
    cost -= d/params.speed * params.free_hour_cost; // cost of time
    cost -= d*params.free_km_cost;                  // cost of km

    unsigned int arriving_time = previous.finish_time + d * 60 / params.speed;
    if (arriving_time > current.start_time)
        return std::nullopt;

    cost -= (current.start_time - arriving_time) * params.wait_cost / 60; // waiting time
    return cost;
}

double Data::GetRealOrderRevenue(size_t ind) const {
    Order order = orders.GetOrder(ind);
    double cost = 0.;

    cost -= (order.finish_time - order.start_time) * params.duty_km_cost; // cost of time  
    cost -= order.distance * params.duty_km_cost;                         // cost of km
    cost += order.revenue;                                                // revenue
    return cost;
}

double Data::GetFreeMovementOrderRevenue(size_t ind) const {
    Order order = orders.GetOrder(ind);
    assert(order.order_id == 0);
    double cost = 0.;

    cost -= (order.finish_time - order.start_time) * params.free_hour_cost; // cost of time  
    cost -= order.distance * params.free_km_cost;                           // cost of km
    assert(order.revenue == 0.);                                            // free movement orders dont have revenue
    return cost;
}

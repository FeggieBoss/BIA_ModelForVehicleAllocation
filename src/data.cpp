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

Data::Data(const Data& other): 
    params(other.params), 
    trucks(other.trucks),
    orders(other.orders),
    dists(other.dists),
    id_to_real_city(other.id_to_real_city),
    cities_count(cities_count) {}

void Data::ShiftTimestamps() {
    min_timestamp = UINT32_MAX;
    for (auto& truck : trucks) {
        min_timestamp = std::min(min_timestamp, truck.init_time);
    }
    for (auto& order : orders) {
        min_timestamp = std::min(min_timestamp, order.start_time);
    }

    for (auto& truck : trucks) {
        truck.init_time -= min_timestamp;
    }
    for (auto& order : orders) {
        order.start_time -= min_timestamp;
        order.finish_time -= min_timestamp;
    }
}

void Data::SqueezeCitiesIds() {
    std::unordered_map<unsigned int, unsigned int> real_city_to_id;
    unsigned int cur = 1;
    auto squeeze_city = [&cur, &real_city_to_id, this](unsigned int real_city) -> unsigned int {
        // havent encountered this city yet
        auto it = real_city_to_id.find(real_city);
        if (it == real_city_to_id.end()) {
            id_to_real_city[cur] = real_city;
            real_city_to_id[real_city] = cur;
            return cur++;
        }
        return it->second;
    };

    for (Truck& truck : trucks) {
        truck.init_city = squeeze_city(truck.init_city);
    }
    for (Order& order : orders) {
        order.from_city = squeeze_city(order.from_city);
        order.to_city = squeeze_city(order.to_city);
    }

    std::map<std::pair<unsigned int,unsigned int>, double> squeezed_dists;
    for (auto& [key, val] : dists.dists) {
        auto& [from, to] = key;
        squeezed_dists.emplace(std::make_pair(squeeze_city(from), squeeze_city(to)), val);
    }
    dists.dists = squeezed_dists;

    cities_count = id_to_real_city.size();
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

    cost -= (order.finish_time - order.start_time) * params.duty_km_cost / 60; // cost of time  
    cost -= order.distance * params.duty_km_cost;                              // cost of km
    cost += order.revenue;                                                     // revenue
    return cost;
}

double Data::GetFreeMovementCost(double distance) const {
    double cost = 0.;
    cost += distance * params.free_km_cost;
    cost += (distance / params.speed) * params.duty_hour_cost;
    return cost;
}

double Data::GetWaitingCost(double mins) const {
    return mins * params.wait_cost / 60.;
}

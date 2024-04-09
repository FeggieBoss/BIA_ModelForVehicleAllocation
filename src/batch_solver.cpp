#include "batch_solver.h"

BatchSolver::BatchSolver() {}

template <class T>
struct cmp {
    bool operator() (const std::pair<unsigned int, T>& a, const std::pair<unsigned int, T>& b) const {
        return a.first < b.first;
    }
};

template <class T>
void GetBatch(
    std::vector<T>& batch,
    unsigned int time_bound,
    std::multiset<std::pair<unsigned int, T>, cmp<T>>& obj_by_init_time
) {
    while(!obj_by_init_time.empty()) {
        auto it = obj_by_init_time.begin();

        // checking if our object 'belongs' to current time window 
        unsigned int time = it->first;
        if (time < time_bound) {
            batch.emplace_back(std::move(it->second));
            obj_by_init_time.erase(it);
        } else {
            break;
        }
    }
}

static void GetCitiesWightsVector(
    CitiesWeightsVectors& cities_ws,
    const Data& data,
    const std::vector<Truck>& batch_trucks,
    const std::vector<Order>& batch_orders,
    const std::multiset<std::pair<unsigned int, Order>, cmp<Order>>& suf_orders
) {
    cities_ws.Reset();
    for (const auto& [_, future_order] : suf_orders) {
        unsigned int to_city = future_order.from_city;

        for (const auto& truck : batch_trucks) {
            if (!IsExecutableBy(
                truck.mask_load_type, 
                truck.mask_trailer_type, 
                future_order.mask_load_type, 
                future_order.mask_trailer_type)
            ) { // bad trailer or load type
                continue;
            }

            auto update_cities_ws = [&future_order, &truck, &cities_ws, &data, to_city](
                unsigned int from_city, 
                unsigned int start_time, 
                unsigned int order_id) 
            {
                auto distance = data.dists.GetDistance(from_city, to_city);
                if (!distance.has_value()) {
                    // there is no road between this two cities
                    return;
                }
                double d = distance.value();
                
                unsigned int finish_time = start_time + (d * 60. / data.params.speed);
                if (finish_time > future_order.start_time) {
                    // no way to get there in time
                    return;
                }

                double weight = future_order.revenue;
                weight -= data.GetFreeMovementCost(d);
                weight -= data.GetWaitingCost(future_order.start_time - finish_time);

                cities_ws.AddWeight(to_city, truck.truck_id, order_id, weight);
            };

            // processing all real last_orders
            for (const Order& last_order : batch_orders) {
                if (!IsExecutableBy(
                    truck.mask_load_type, 
                    truck.mask_trailer_type, 
                    last_order.mask_load_type, 
                    last_order.mask_trailer_type)
                ) { // bad trailer or load type
                    return;
                }
                update_cities_ws(last_order.to_city, last_order.finish_time, last_order.order_id);
            }

            // processing our last order is ffo <=> we wont make any orders on current batch at all
            {
                update_cities_ws(truck.init_city, truck.init_time, 0);
            }
        }
    }
}

solution_t BatchSolver::Solve(const Data& data, unsigned int time_window) {
    // main Data
    Params params = data.params;
    Trucks trucks = data.trucks;
    Orders orders = data.orders;
    Distances dists = data.dists;

    size_t trucks_count = trucks.Size();
    size_t orders_count = orders.Size();

    // Solution
    solution_t main_solution {
        std::vector<std::vector<size_t>>(trucks_count, std::vector<size_t>())
    };

    // Showing object position in main Data by its id
    std::unordered_map<unsigned int, size_t> truck_pos_by_id;
    for (size_t truck_pos = 0; truck_pos < trucks_count; ++truck_pos) {
        truck_pos_by_id[trucks.GetTruckConst(truck_pos).truck_id] = truck_pos;
    }
    std::unordered_map<unsigned int, size_t> order_pos_by_id;
    for (size_t order_pos = 0; order_pos < orders_count; ++order_pos) {
        order_pos_by_id[orders.GetOrderConst(order_pos).order_id] = order_pos;
    }

    // Avaible trucks and orders
    std::multiset<std::pair<unsigned int, Truck>, cmp<Truck>> trucks_by_init_time;
    for (const Truck& truck : trucks) {
        trucks_by_init_time.emplace(truck.init_time, truck);
    }
    std::multiset<std::pair<unsigned int, Order>, cmp<Order>> order_by_start_time;
    for (const Order& order : orders) {
        order_by_start_time.emplace(order.start_time, order);
    }

    // Current batches of trucks/orders
    std::vector<Truck> batch_trucks;
    std::vector<Order> batch_orders;

    CitiesWeightsVectors cities_ws(data.cities_count + 1);
    WeightedCitiesSolver solver;
    for(unsigned int cur_time_window = time_window;; cur_time_window += time_window) {
        // check if we processed all orders
        if (order_by_start_time.empty()) {
            break;
        }

        // Adding trucks/orders to current batches
        GetBatch<Truck>(batch_trucks, cur_time_window, trucks_by_init_time);
        GetBatch<Order>(batch_orders, cur_time_window, order_by_start_time);

        // we suppose to double our time segment and try grow batches  
        if (batch_trucks.empty() || batch_orders.empty()) {
            continue;
        }
        // batches is okay here so we should sovle sub-problem on them and realese later 

        // Initializing data for sub-problem
        Data batch_data(data);
        batch_data.trucks = batch_trucks;
        batch_data.orders = batch_orders;
        
        // Solving problem with current batches
        GetCitiesWightsVector(cities_ws, data, batch_trucks, batch_orders, order_by_start_time);
        solver.SetData(batch_data, cur_time_window, cities_ws);
        solution_t batch_solution = solver.Solve();

        // We want to work with free-movement orders (read Note in weighted_cities_solver.h)
        const Data& modified_batch_data = solver.GetDataConst();

        // Merging solutions
        size_t batch_trucks_count = modified_batch_data.trucks.Size();
        for (size_t batch_truck_pos = 0; batch_truck_pos < batch_trucks_count; ++batch_truck_pos) {
            const Truck& truck = modified_batch_data.trucks.GetTruckConst(batch_truck_pos);

            for (size_t batch_order_pos : batch_solution.orders_by_truck_pos[batch_truck_pos]) {
                const Order& order = modified_batch_data.orders.GetOrderConst(batch_order_pos);

                unsigned int order_id = order.order_id;
                // checking if its real order (not free-movement edge)
                if (order_id > 0) {
                    size_t real_truck_pos = truck_pos_by_id[truck.truck_id];
                    size_t real_order_pos = order_pos_by_id[order_id];
                    main_solution.orders_by_truck_pos[real_truck_pos].push_back(real_order_pos);
                }
            }
        }

        // Adding trucks back with new configurations
        for (size_t batch_truck_pos = 0; batch_truck_pos < batch_trucks_count; ++batch_truck_pos) {
            Truck cur_truck = modified_batch_data.trucks.GetTruck(batch_truck_pos);

            const auto& cur_orders = batch_solution.orders_by_truck_pos[batch_truck_pos];
            if (cur_orders.empty()) {
                cur_truck.init_time = cur_time_window;
            } else {
                const Order& order = modified_batch_data.orders.GetOrderConst(cur_orders.back());
                cur_truck.init_time = order.finish_time;
                cur_truck.init_city = order.to_city;
            }

            trucks_by_init_time.emplace(cur_truck.init_time, std::move(cur_truck));
        }

        // Releasing old batches
        batch_trucks.clear();
        batch_orders.clear();
    }

    return main_solution;
}
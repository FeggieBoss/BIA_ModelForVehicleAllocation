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

static void UpdateFreeMovementWeightsVectors(
    FreeMovementWeightsVectors& edges_w_vecs,
    const Data& batch_data,
    unsigned int time_bound,
    unsigned int time_window,
    const std::multiset<std::pair<unsigned int, Order>, cmp<Order>>& suf_orders
) {
    static constexpr double eps = 1e-6;

    edges_w_vecs.Reset();

    const Trucks& batch_trucks = batch_data.trucks;
    const Orders& batch_orders = batch_data.orders;

    const size_t batch_trucks_count = batch_trucks.Size();
    const size_t batch_orders_count = batch_orders.Size();

    static auto update_edges_w_vecs = [](
        FreeMovementWeightsVectors& edges_w_vecs,
        const Data& batch_data,
        const Truck& truck,
        const Order& from_order,
        const Order& to_order,
        size_t truck_pos,
        size_t order_pos)
    {
        unsigned int to_city = to_order.from_city;

        if (auto raw_bonus = batch_data.MoveBetweenOrders(truck, from_order, to_order)) {
            double bonus = raw_bonus.value();
            if (to_order.obligation) {
                // TO DO
                bonus = std::max(bonus, 2*eps);
            }
            if (bonus >= eps) {
                edges_w_vecs.AddWeight(truck_pos, order_pos, to_city, bonus);
            }
        }
    };

    /*
        Each truck will either
        (1) do at least one order
        (2) or will stay at initial city

        So what we really want to do here is just add free-movement edges from to_city of some order or init_city of some truck
        to some future_order.from_city if its really 'worth' doing.
        
        Note: solver wont use one of such edges twice but its still goin to work
        (1) because each truck has its own last order by same reasons and we will add edges for this order
        (2) we will add multiple edges for some cities which is init_city for more than one truck
    */
    for (size_t truck_pos = 0; truck_pos < batch_trucks_count; ++truck_pos) {
        const Truck& truck = batch_trucks.GetTruckConst(truck_pos);

        for (size_t order_pos = 0; order_pos < batch_orders_count; ++order_pos) {
            const Order& last_order = batch_orders.GetOrderConst(order_pos);

            // there is no point in free-movement edges when last order finishes after next batch will start
            if (last_order.finish_time >= time_bound) {
                continue;
            } else if (!IsExecutableBy(
                truck.mask_load_type, 
                truck.mask_trailer_type, 
                last_order.mask_load_type, 
                last_order.mask_trailer_type)
            ) { // bad trailer or load type
                continue;
            }

            for (const auto& [_, future_order] : suf_orders) {
                /*
                    we need only orders that belong to next batch
                    Note: working fast because we are using break here(orders are sorted by start_time)
                */
                if (future_order.start_time >= time_bound + time_window) {
                    break;
                }
                update_edges_w_vecs(edges_w_vecs, batch_data, truck, last_order, future_order, truck_pos, order_pos);
            }
        }

        for (const auto& [_, future_order] : suf_orders) {
            /*
                we need only orders that belong to next batch
                Note: working fast because we are using break here(orders are sorted by start_time)
            */
            if (future_order.start_time >= time_bound + time_window) {
                break;
            }
            // processing our last order is ffo <=> we wont make any orders on current batch at all
            {
                const Order& ffo = Solver::make_ffo(truck);
                update_edges_w_vecs(edges_w_vecs, batch_data, truck, ffo, future_order, truck_pos, Solver::ffo_pos);
            }
        }
    }
}

static void FilterSuffix(
    std::multiset<std::pair<unsigned int, Truck>, cmp<Truck>>& trucks_by_init_time,
    std::multiset<std::pair<unsigned int, Order>, cmp<Order>>& order_by_start_time
) {
    assert(!trucks_by_init_time.empty());

    unsigned int min_init_time = trucks_by_init_time.begin()->first;
    while (!order_by_start_time.empty()) {
        auto it = order_by_start_time.begin();
        if (it->first < min_init_time) {
            order_by_start_time.erase(it);
        } else {
            break;
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
    // Making sure we dont have useless orders
    FilterSuffix(trucks_by_init_time, order_by_start_time);

    // Current batches of trucks/orders
    std::vector<Truck> batch_trucks;
    std::vector<Order> batch_orders;

    FreeMovementWeightsVectors edges_w_vecs;
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

        /*
            Same additional bits will be used later in LOAD_TYPE of free-movement edges of this truck 
            this will block other trucks from picking this edges => reduce amount of variables in LP model
        */
        for (size_t batch_truck_pos = 0; batch_truck_pos < batch_data.trucks.Size(); ++batch_truck_pos) {
            Truck& truck = trucks.GetTruckRef(batch_truck_pos);
            truck.mask_load_type += batch_truck_pos * (1 << LOAD_TYPE_COUNT);
        }

        std::cout << "BATCH_DEBUG: " << batch_data.trucks.Size() << " " << batch_data.orders.Size() << std::endl;

        #ifdef DEBUG_MODE
        std::cout << "##BATCH_DEBUG" << std::endl;
        batch_data.trucks.DebugPrint();
        batch_data.orders.DebugPrint();
        std::cout << "BATCH_DEBUG##" << std::endl << std::endl;
        #endif   
        
        // Solving problem with current batches
        UpdateFreeMovementWeightsVectors(edges_w_vecs, batch_data, cur_time_window, time_window, order_by_start_time);
        solver.SetData(batch_data, cur_time_window, edges_w_vecs);
        solution_t batch_solution = solver.Solve();

        // We want to work with free-movement orders (read Note in weighted_cities_solver.h)
        const Data& modified_batch_data = solver.GetDataConst();

        std::cout << "BATCH_DEBUG: with additional orders (" << modified_batch_data.orders.Size() << ")" << std::endl;

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


        /*
            Making sure we dont have useless orders 
            Why do we call it again? - trucks init_time's just changed so we have different state right now
        */
        FilterSuffix(trucks_by_init_time, order_by_start_time);
    }

    return main_solution;
}
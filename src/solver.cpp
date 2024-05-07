#include "solver.h"

////////////////////////////////
// FreeMovementWeightsVectors //
////////////////////////////////

std::pair<Orders, std::unordered_map<std::tuple<size_t, size_t, unsigned int>, size_t>> FreeMovementWeightsVectors::GetFreeMovementEdges(const Data& data) const {
    static auto AddFreeMovementEdge = [](
        Orders& orders,
        const Params& params,
        const Distances& dists,
        unsigned int from_city, 
        unsigned int to_city, 
        unsigned int start_time,
        int mask_load_type, 
        int mask_trailer_type, 
        double revenue_bonus)
    {
        double d = dists.GetDistance(from_city, to_city).value();
        double time_hours = d / params.speed;
        unsigned int finish_time = start_time + (d * 60. / params.speed);
        /*
            later in solver we will treat such orders as real ones 
            why? - checker or other components of pipeline ideally shouldnt know anythin except main rules of task 
            (because we dont wanna change their code at all after adding new solvers with new ModifyData's)
            
            So we will calculate real order revenue by subtracting duty_time_cost, duty_km_cost
            but in reality we suppose to use free_time_cost, free_km_cost thus we need to compensate such error
            Note: revenue_bonus supposed to already subtract free_time_cost and free_km_cost
        */
        double revenue = 0.f;
        // compensating error
        revenue += d * params.duty_km_cost;
        revenue += time_hours * params.duty_hour_cost;

        // taking in account that this edge will have additional bonus by weights vector
        revenue += revenue_bonus;

        // if we are creating loop lets prohibit truck from using it and then some other free-movement edge
        if (start_time == finish_time) {
            finish_time = start_time + 1;
        }

        Order new_order = {
            0,                          // order_id
            false,                      // obligation
            start_time,                 // start_time,
            finish_time,                // finish_time
            from_city,                  // from_city
            to_city,                    // to_city
            mask_load_type,             // mask_load_type
            mask_trailer_type,          // mask_trailer_type
            d,                          // distance
            revenue                     // revenue
        };
        
        orders.AddOrder(new_order);
    };

    // will return this orders and mapping
    Orders dop_orders;
    std::unordered_map<std::tuple<size_t, size_t, unsigned int>, size_t> edge_to_pos;

    const Orders& orders = data.orders;
    const Params& params = data.params;
    const Trucks& trucks = data.trucks;
    const Distances& dists  = data.dists;

    /* 
        There is no point to add free-movement edges twice
        also each free-movement edge can be identified by {from_city, to_city, start_time}

        Additionally store position of free-movement edge in orders set
    */
    std::unordered_map<std::tuple<unsigned int, unsigned int, unsigned int>, size_t> used;

    for (const auto& [gkey, vec] : *this) {
        const auto& [truck_pos, order_pos] = gkey;

        for(const auto& [to_city, revenue_bonus] : vec) {
            unsigned int from_city;
            unsigned int start_time;

            const Truck& truck = trucks.GetTruckConst(truck_pos);
            if (order_pos == Solver::ffo_pos) {
                from_city = truck.init_city;
                start_time = truck.init_time;
            } else {
                const Order& order = orders.GetOrderConst(order_pos);
                from_city = order.to_city;
                start_time = order.finish_time;
            }
            
            std::tuple<unsigned int, unsigned int, unsigned int> tuple_key = {from_city, to_city, start_time};
            // checking if we already tried to add this edge
            {
                auto it = used.find(tuple_key);
                if (it != used.end()) {
                    edge_to_pos[{truck_pos, order_pos, to_city}] = it->second;
                    continue;
                }
            }

            // made for performance boost purposes - making free-movement edge pickable only by truck with 'truck_pos'
            int mask_load_type = truck.mask_load_type + truck_pos * (1 << LOAD_TYPE_COUNT);
            AddFreeMovementEdge(
                dop_orders,
                params,
                dists,
                from_city,
                to_city,
                start_time,
                mask_load_type,
                truck.mask_trailer_type,
                revenue_bonus
            );

            edge_to_pos[{truck_pos, order_pos, to_city}] = used[tuple_key] = dop_orders.Size() - 1;
        }
    }

    return {dop_orders, edge_to_pos};
}

FreeMovementWeightsVectors::FreeMovementWeightsVectors() {};

FreeMovementWeightsVectors::FreeMovementWeightsVectors(const FreeMovementWeightsVectors& other): edges_w_vecs_(other.edges_w_vecs_) {}

const FreeMovementWeightsVectors& FreeMovementWeightsVectors::operator=(const FreeMovementWeightsVectors& other) {
    edges_w_vecs_ = other.edges_w_vecs_;
    return *this;
}

bool FreeMovementWeightsVectors::IsInitialized() const {
    return !edges_w_vecs_.empty();
}


std::optional<std::reference_wrapper<weights_vector_t>> FreeMovementWeightsVectors::GetWeightsVector(size_t truck_pos, size_t order_pos) {
    auto it = edges_w_vecs_.find({truck_pos, order_pos});
    if (it == edges_w_vecs_.end()) {
        return std::nullopt;
    }
    return std::optional(std::ref(it->second));
}

std::optional<std::reference_wrapper<const weights_vector_t>> FreeMovementWeightsVectors::GetWeightsVectorConst(size_t truck_pos, size_t order_pos) const {
    auto it = edges_w_vecs_.find({truck_pos, order_pos});
    if (it == edges_w_vecs_.end()) {
        return std::nullopt;
    }
    return std::optional(std::cref(it->second));
}

std::optional<double> FreeMovementWeightsVectors::GetWeight(size_t truck_pos, size_t order_pos, unsigned int city_id) const {
    if (auto raw_vec = GetWeightsVectorConst(truck_pos, order_pos)) {
        const weights_vector_t& vec = raw_vec.value();

        auto it = vec.find(city_id);
        if (it == vec.end()) {
            return std::nullopt;
        }
        return {it->second};
    } else {
        return std::nullopt;
    }
}

void FreeMovementWeightsVectors::AddWeight(size_t truck_pos, size_t order_pos, unsigned int city_id, double weight) {
    if (auto raw_vec = GetWeightsVector(truck_pos, order_pos)) {
        weights_vector_t& vec = raw_vec.value();
        // (double)0 is default value so += will work if there is no such element yet
        vec[city_id] += weight;
    } else {
        edges_w_vecs_[{truck_pos, order_pos}][city_id] = weight;
    }
}

void FreeMovementWeightsVectors::Reset() {
    edges_w_vecs_.clear();
}

////////////
// SOLVER //
////////////

size_t Solver::ffo_pos = static_cast<size_t>(-1);
size_t Solver::flo_pos = static_cast<size_t>(-2);
std::function<Order(const Truck&)> Solver::make_ffo = [] (const Truck& truck) -> Order {
    Order ffo;
    ffo.to_city = truck.init_city;
    ffo.finish_time = truck.init_time;
    return ffo;
};

std::function<Order(const Order&)> Solver::make_flo = [] (const Order& from_order) -> Order {
    Order flo;
    flo.from_city = from_order.to_city;
    flo.start_time = from_order.finish_time;
    return flo;
};


bool Solver::IsFakeOrder(size_t order_pos) {
    return (order_pos == ffo_pos || order_pos == flo_pos);
}

std::vector<size_t> Solver::Solve(HighsModel& model) {
    std::cout << "Model(" << model.lp_.num_col_ << ',' << model.lp_.num_row_ << ")\n";
    if (model.lp_.num_col_ == 0) {
        return {};
    }

    Highs highs;
    #ifndef DEBUG_MODE
    highs.setOptionValue("output_flag", false);
    #endif
    HighsStatus return_status;
    
    return_status = highs.passModel(model);
    assert(return_status==HighsStatus::kOk);

    const HighsLp& lp = highs.getLp(); 

    return_status = highs.run();
    assert(return_status==HighsStatus::kOk);
    
    const HighsModelStatus& model_status = highs.getModelStatus();
    assert(model_status==HighsModelStatus::kOptimal);
    
    const HighsInfo& info = highs.getInfo();
    #ifdef DEBUG_MODE
    cout << "Model status: " << highs.modelStatusToString(model_status) << endl
        << "Simplex iteration count: " << info.simplex_iteration_count << endl
        << "Objective function value: " << info.objective_function_value << endl
        << "Primal  solution status: " << highs.solutionStatusToString(info.primal_solution_status) << endl
        << "Dual    solution status: " << highs.solutionStatusToString(info.dual_solution_status) << endl
        << "Basis: " << highs.basisValidityToString(info.basis_validity) << endl;
    #endif
    
    const HighsSolution& solution = highs.getSolution();
    
    model.lp_.integrality_.resize(lp.num_col_);
    for (int col=0; col < lp.num_col_; col++)
        model.lp_.integrality_[col] = HighsVarType::kInteger;

    highs.passModel(model);
    
    return_status = highs.run();
    assert(return_status==HighsStatus::kOk);

    std::vector<size_t> setted_columns;
    setted_columns.reserve(lp.num_col_);
    for (int col = 0; col < lp.num_col_; ++col) {
        if (info.primal_solution_status && solution.col_value[col]) {
            setted_columns.push_back(col);
        }
    }
    return setted_columns;
}
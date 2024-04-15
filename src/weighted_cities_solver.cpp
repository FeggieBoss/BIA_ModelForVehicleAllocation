#include "weighted_cities_solver.h"

////////////////////////////////
// FreeMovementWeightsVectors //
////////////////////////////////

FreeMovementWeightsVectors::FreeMovementWeightsVectors() {};

FreeMovementWeightsVectors::FreeMovementWeightsVectors(size_t cities_count) {
    edges_w_vecs_.resize(cities_count + 1);
}

FreeMovementWeightsVectors::FreeMovementWeightsVectors(const FreeMovementWeightsVectors& other): edges_w_vecs_(other.edges_w_vecs_) {}

const FreeMovementWeightsVectors& FreeMovementWeightsVectors::operator=(const FreeMovementWeightsVectors& other) {
    edges_w_vecs_ = other.edges_w_vecs_;
    return *this;
}

bool FreeMovementWeightsVectors::IsInitialized() const {
    return !edges_w_vecs_.empty();
}

std::optional<double> FreeMovementWeightsVectors::GetWeight(unsigned int city_id, size_t truck_pos, size_t order_pos) const {
    assert(edges_w_vecs_.size() > city_id);
    const auto& vec = edges_w_vecs_[city_id];
    auto it = vec.find({truck_pos, order_pos});
    if (it == vec.end()) {
        return std::nullopt;
    }
    return {it->second};
}

const weights_vector_t& FreeMovementWeightsVectors::GetWeightsVector(unsigned int city_id) const {
    assert(edges_w_vecs_.size() > city_id);
    const auto& vec = edges_w_vecs_[city_id];
    return vec;
}

void FreeMovementWeightsVectors::AddWeight(unsigned int city_id, size_t truck_pos, size_t order_pos, double weight) {
    assert(edges_w_vecs_.size() > city_id);
    const auto& vec = edges_w_vecs_[city_id];
    auto it = vec.find({truck_pos, order_pos});
    if (it == vec.end()) {
        edges_w_vecs_[city_id][{truck_pos, order_pos}] = weight;
    } else {
        edges_w_vecs_[city_id][{truck_pos, order_pos}] += weight;
    }
}

void FreeMovementWeightsVectors::Reset() {
    for (auto &vec : edges_w_vecs_) {
        vec.clear();
    }
}


//////////////////////////
// WeightedCitiesSolver //
//////////////////////////

WeightedCitiesSolver::WeightedCitiesSolver() {}

const Data& WeightedCitiesSolver::GetDataConst() const {
    return Solver::data_;
}

void WeightedCitiesSolver::ModifyData(Data& data) const {
    Orders& orders = data.orders;
    const Params& params = data.params;
    const Trucks& trucks = data.trucks;
    const Distances& dists  = data.dists;
    auto add_edges = [&orders, &params, &dists/*, &all_good_cities*/](
        unsigned int from_city, 
        unsigned int to_city, 
        unsigned int start_time,
        double revenue_bonus) {
            
        if (from_city == to_city) return;

        auto distance = dists.GetDistance(from_city, to_city);
        if (!distance.has_value()) {
            // there is no road between this two cities
            return;
        }
        double d = distance.value();
        double time_hours = d / params.speed;
        unsigned int finish_time = start_time + (d * 60. / params.speed);
        /*
            later in solver we will treat such orders as real ones 
            why? - checker or other components of pipeline ideally shouldnt know anythin except main rules of task 
            (because we dont wanna change their code at all after adding new solvers with new ModifyData's)
            
            So we will calculate real order revenue by subtracting duty_time_cost, duty_km_cost
            but in reality we suppose to use free_time_cost, free_km_cost thus we need to compensate such error
        */
        double revenue = 0.f;
        // compensating error
        revenue += d * params.duty_km_cost;
        revenue += time_hours * params.duty_hour_cost;
        // subtracting real cost
        revenue -= d * params.free_km_cost;
        revenue -= time_hours * params.free_hour_cost;

        // taking in account that this edge will have additional bonus by weights vector
        revenue += revenue_bonus;

        Order new_order = {
            0,                          // order_id
            false,                      // obligation
            start_time,                 // start_time,
            finish_time,                // finish_time
            from_city,                  // from_city
            to_city,                    // to_city
            GetFullMaskLoadType(),      // mask_load_type
            GetFullMaskTrailerType(),   // mask_trailer_type
            d,                          // distance
            revenue                     // revenue
        };
        
        orders.AddOrder(new_order);
    };

    for (unsigned int to_city = 1; to_city <= data.cities_count; ++to_city) {
        const auto& weights_vec = cities_ws_.GetWeightsVector(to_city);
        for (const auto& [key, w] : weights_vec) {
            const auto& [truck_pos, order_pos] = key;

            unsigned int from_city;
            unsigned int start_time;
            double revenue_bonus = w;

            if (order_pos == Solver::ffo_pos) {
                const Truck& truck = trucks.GetTruckConst(truck_pos);
                from_city = truck.init_city;
                start_time = truck.init_time;
            } else {
                const Order& order = orders.GetOrderConst(order_pos);
                from_city = order.to_city;
                start_time = order.finish_time;
            }
            add_edges(from_city, to_city, start_time, revenue_bonus);
        }
    }

    #ifdef DEBUG_MODE
    cout << "ADDED FREE-MOVEMENT-ORDERS:"<<endl;
    data.orders.DebugPrint();
    #endif
}

void WeightedCitiesSolver::SetData(const Data& data) {
    Solver::data_ = data;
    if (!cities_ws_.IsInitialized()) {
        cities_ws_ = FreeMovementWeightsVectors(data.cities_count);
    }
    ModifyData(Solver::data_);
}

void WeightedCitiesSolver::SetData(const Data& data, unsigned int t, const FreeMovementWeightsVectors& cities_ws) {
    cities_ws_ = cities_ws;
    time_boundary_ = { t };
    SetData(data);
}

HighsModel WeightedCitiesSolver::CreateModel() {
    Solver::to_3d_variables_.clear();

    Params params = Solver::data_.params;
    Trucks trucks = Solver::data_.trucks;
    Orders orders = Solver::data_.orders;    
    Distances dists = Solver::data_.dists;

    HonestSolver solver;
    solver.SetData(Solver::data_);
    HighsModel model = solver.CreateModel();

    Solver::to_3d_variables_ = solver.to_3d_variables_;

    // changing CostVector: applying fine for waiting until time_boundary
    if (time_boundary_.has_value()) {
        unsigned int time_boundary = time_boundary_.value();
        size_t orders_count = orders.Size();  

        // processing c
        // model.lp_.col_cost_
        for (auto &el : Solver::to_3d_variables_) {
            size_t ind = el.first;
            auto& [truck_pos, from_order_pos, to_order_pos] = el.second;

            double fine = 0.;
            if (to_order_pos == Solver::flo_pos) {
                const Truck& truck = trucks.GetTruckConst(truck_pos);
                const Order& from_order = (from_order_pos != Solver::ffo_pos ? orders.GetOrderConst(from_order_pos) : Solver::make_ffo(truck));

                /*
                    lets add fine for waiting until time_boudary
                    there is possibility that we will finish our order after time_boundary
                    Note: we only expected order.start_time < time_boundary
                */
                if (time_boundary > from_order.finish_time) {
                    fine += (time_boundary - from_order.finish_time) * params.wait_cost / 60;
                }
            }

            model.lp_.col_cost_[ind] -= fine;
        }
    }
    #ifdef DEBUG_MODE
    cout << "##COST_VECTOR_DEBUG" << endl; 
    for (size_t ind = 0; ind < model.lp_.col_cost_.size(); ++ind) {
        auto& [i,j,k] = Solver::to_3d_variables_[ind];
        double c = model.lp_.col_cost_[ind];

        printf("C[{%2ld, %2ld, %2ld}] = %5f\n", i, j, k, c);
    }
    cout << "COST_VECTOR_DEBUG##" << endl; 
    #endif

    return model;
}
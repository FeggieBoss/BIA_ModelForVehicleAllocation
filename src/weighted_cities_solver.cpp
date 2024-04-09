#include "weighted_cities_solver.h"

//////////////////////////
// CitiesWeightsVectors //
//////////////////////////

CitiesWeightsVectors::CitiesWeightsVectors() {};

CitiesWeightsVectors::CitiesWeightsVectors(size_t cities_count) {
    cities_w_vecs_.resize(cities_count);
}

CitiesWeightsVectors::CitiesWeightsVectors(const CitiesWeightsVectors& other): cities_w_vecs_(other.cities_w_vecs_) {}

const CitiesWeightsVectors& CitiesWeightsVectors::operator=(const CitiesWeightsVectors& other) {
    cities_w_vecs_ = other.cities_w_vecs_;
    return *this;
}

bool CitiesWeightsVectors::IsInitialized() const {
    return !cities_w_vecs_.empty();
}

std::optional<double> CitiesWeightsVectors::GetWeight(unsigned int city_id, unsigned int truck_id, unsigned int order_id) const {
    assert(cities_w_vecs_.size() > city_id);
    const auto& vec = cities_w_vecs_[city_id];
    auto it = vec.find({truck_id, order_id});
    if (it == vec.end()) {
        return std::nullopt;
    }
    return {it->second};
}

const weights_vector_t& CitiesWeightsVectors::GetWeightsVector(unsigned int city_id) const {
    const auto& vec = cities_w_vecs_[city_id];
    return vec;
}

void CitiesWeightsVectors::AddWeight(unsigned int city_id, unsigned int truck_id, unsigned int order_id, double weight) {
    assert(cities_w_vecs_.size() > city_id);
    const auto& vec = cities_w_vecs_[city_id];
    auto it = vec.find({truck_id, order_id});
    if (it == vec.end()) {
        cities_w_vecs_[city_id][{truck_id, order_id}] = weight;
    } else {
        cities_w_vecs_[city_id][{truck_id, order_id}] += weight;
    }
}

void CitiesWeightsVectors::Reset() {
    for (auto &vec : cities_w_vecs_) {
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
    // cities with positive weight
    std::set<unsigned int> all_good_cities;
    for (unsigned int city = 1; city <= data.id_to_real_city.size(); ++city) {
        const auto& weights_vec = cities_ws_.GetWeightsVector(city);
        if (!weights_vec.empty()) {
            all_good_cities.emplace(city);
        }
    }

    /*
        Each truck will either
        (1) do at least one order
        (2) or will stay at initial city
        so we will add edges for to_city of every order and init_city each truck
        
        Note: solver wont use one of such edges twice but its still goin to work
        (1) because each truck has its own last order by same reasons and we will add edges for this order
        (2) we will add multiple edges for some cities which is init_city for more than one truck
    */

    auto& params = data.params;
    auto& orders = data.orders;
    auto& dists  = data.dists;
    auto add_edges = [&params, &orders, &dists, &all_good_cities](unsigned int from_city, unsigned int start_time) {
        for(unsigned int to_city : all_good_cities) {
            if (from_city == to_city) continue;

            auto distance = dists.GetDistance(from_city, to_city);
            if (!distance.has_value()) {
                // there is no road between this two cities
                continue;
            }
            double d = distance.value();
            
            unsigned int finish_time = start_time + (d * 60 / params.speed);
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
            revenue += (finish_time - start_time) * params.duty_hour_cost;
            // subtracting real cost
            revenue -= d * params.free_km_cost;
            revenue -= (finish_time - start_time) * params.free_hour_cost;

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
        }
    };

    // adding free-movement edges from to_city of each order
    for (auto &order : data.orders) {
        // city we are in after completing order / city we start our free move from
        unsigned int from_city = order.to_city;
        unsigned int start_time = order.finish_time;

        add_edges(from_city, start_time);
    }

    // adding free-movement edges from init_city of each truck
    for (auto &truck : data.trucks) {
        unsigned int from_city = truck.init_city;
        unsigned int start_time = truck.init_time;

        add_edges(from_city, start_time);
    }

    #ifdef DEBUG_MODE
    cout << "ADDED FREE-MOVEMENT-ORDERS:"<<endl;
    data.orders.DebugPrint();
    #endif
}

void WeightedCitiesSolver::SetData(const Data& data) {
    Solver::data_ = data;
    if (!cities_ws_.IsInitialized()) {
        cities_ws_ = CitiesWeightsVectors(data.cities_count + 1);
    }
    ModifyData(Solver::data_);
}

void WeightedCitiesSolver::SetData(const Data& data, unsigned int t, const CitiesWeightsVectors& cities_ws) {
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

    // changing CostVector: applying weights of cities + fine for waiting until time_boundary
    {
        size_t orders_count = orders.Size();  

        size_t fake_first_order = orders_count;
        size_t fake_last_order = orders_count + 1;
        static auto make_ffo = [] (const Truck& truck) -> Order {
            Order ffo;
            ffo.to_city = truck.init_city;
            ffo.finish_time = truck.init_time;
            return ffo;
        };

        // processing c
        // model.lp_.col_cost_
        // model.lp_.col_cost_.resize(model.lp_.num_col_);
        for (auto &el : Solver::to_3d_variables_) {
            size_t ind = el.first;
            auto& [i,j,k] = el.second;
            const Truck& truck = trucks.GetTruckConst(i);

            const Order& from_order = (j != fake_first_order ? orders.GetOrderConst(j) : make_ffo(truck));

            double c = 0.;
            if (k == fake_last_order) {
                /*
                    taking in account weight of last city in scheduling scheme of this truck
                    if from_order = ffo => its order_id = 0 !!!
                */
                auto weight = cities_ws_.GetWeight(from_order.to_city, truck.truck_id, from_order.order_id);
                c += (weight.has_value() ? weight.value() : 0.);

                // also lets add fine for waiting until time_boudary
                if (time_boundary_.has_value()) {
                    unsigned int time_boundary = time_boundary_.value();
                    /*
                        there is possibility that we will finish our order after time_boundary
                        Note: we only expected order.start_time < time_boundary
                    */
                    if (time_boundary > from_order.finish_time) {
                        c -= (time_boundary - from_order.finish_time) * params.wait_cost / 60;
                    }
                }
            }

            model.lp_.col_cost_[ind] += c;
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
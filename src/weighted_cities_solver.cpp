#include "weighted_cities_solver.h"

// #define NO_LONG_EDGES
namespace std{
    namespace
    {
        // Code from boost
        // Reciprocal of the golden ratio helps spread entropy
        //     and handles duplicates.
        // See Mike Seymour in magic-numbers-in-boosthash-combine:
        //     https://stackoverflow.com/questions/4948780

        template <class T>
        inline void hash_combine(std::size_t& seed, T const& v)
        {
            seed ^= hash<T>()(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
        }

        // Recursive template code derived from Matthieu M.
        template <class Tuple, size_t Index = std::tuple_size<Tuple>::value - 1>
        struct HashValueImpl
        {
          static void apply(size_t& seed, Tuple const& tuple)
          {
            HashValueImpl<Tuple, Index-1>::apply(seed, tuple);
            hash_combine(seed, get<Index>(tuple));
          }
        };

        template <class Tuple>
        struct HashValueImpl<Tuple,0>
        {
          static void apply(size_t& seed, Tuple const& tuple)
          {
            hash_combine(seed, get<0>(tuple));
          }
        };
    }

    template <typename ... TT>
    struct hash<std::tuple<TT...>> 
    {
        size_t
        operator()(std::tuple<TT...> const& tt) const
        {                                              
            size_t seed = 0;                             
            HashValueImpl<std::tuple<TT...> >::apply(seed, tt);    
            return seed;                                 
        }                                              

    };
}

WeightedCitiesSolver::WeightedCitiesSolver() {}

void WeightedCitiesSolver::SetData(const Data& data) {
    data_ = data;

    // its not all cities as name claims :)
    // its rather cities we can move somewhere from which exactly what we need
    std::set<unsigned int> all_cities;
    for (auto& [key, d] : data_.dists.dists) {
        auto& [from, _] = key;
        all_cities.emplace(from);
    }

    for (auto &order : data_.orders) {
        // city we are in after completing order / city we start our free move from
        unsigned int from_city = order.to_city;
        unsigned int start_time = order.finish_time;

        for(unsigned int to_city : all_cities) {
            if (from_city == to_city) continue;

            auto distance = data_.dists.GetDistance(from_city, to_city);
            if (!distance.has_value()) {
                // there is no road between this two cities
                continue;
            }
            double d = distance.value();
            
            unsigned int finish_time = start_time + (d * 60 / data_.params.speed);

            if (free_move_time_boundary_.has_value()) {
                unsigned int time_bound = free_move_time_boundary_.value();
                if (finish_time > time_bound) {
                    // we cant move to such city before we running out of time
                    continue;
                }
            }

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
                0.                          // revenue
            };
            
            data_.orders.AddOrder(new_order);
        }
    }

    #ifdef DEBUG_MODE
    cout << "ADDED FREE-MOVEMENT-ORDERS:"<<endl;
    data_.orders.DebugPrint();
    #endif
}

void WeightedCitiesSolver::SetData(const Data& data, unsigned int t) {
    free_move_time_boundary_ = {t};
    SetData(data);
}

void WeightedCitiesSolver::SetVectorW(const weights_vector_t& w) {
    w_ = w;
}



// static size_t Get1dVariable(size_t i, size_t j, size_t k, size_t orders_count) {
//     auto m = orders_count;
//     return k+(j+i*m)*m;
// } 

static variable_t Get3dVariable(size_t x, size_t orders_count) {
    auto m = orders_count;
    size_t k = (x%m);   
    size_t j = (x/m)%m;
    size_t i = (x/m)/m;
    return {i, j, k};
}

HighsModel WeightedCitiesSolver::CreateModel() {
    Params params = data_.params;
    Trucks trucks = data_.trucks;
    Orders orders = data_.orders;    
    Distances dists = data_.dists;

    size_t trucks_count = trucks.Size();
    size_t orders_count = orders.Size();    

    // c^Tx + d subject to L <= Ax <= U; l <= x <= u
    HighsModel model;
    // maximizing revenue => kMaximize
    model.lp_.sense_ = ObjSense::kMaximize;
    // writing matrix A by rows
    model.lp_.a_matrix_.format_ = MatrixFormat::kRowwise;
    model.lp_.offset_ = 0;


    // processing l,u + storing non zero variables
    // model.lp_.col_lower_, model.lp_.col_upper_
    #ifdef NO_LONG_EDGES
    int no_long_edges = 0;
    #endif

    // variable_t -> its index in vector X
    std::unordered_map<variable_t, size_t> to_index;
    static auto GetVariableIndex = [&to_index] (const variable_t& v) -> std::optional<size_t> {
        auto it = to_index.find(v);
        if (it == to_index.end()) {
            return std::nullopt;
        }
        return it->second;
    };

    // lets calculate l,u for l <= x <= u
    // for every (i,j,k) -> i-th truck will pick k-th order after completing j-th order
    for(size_t i = 0; i < trucks_count; ++i) {
        Truck truck = trucks.GetTruck(i);

        for(size_t j = 0; j < orders_count; ++j) {
            Order from_order = orders.GetOrder(j);

            if (!IsExecutableBy(
                truck.mask_load_type, 
                truck.mask_trailer_type, 
                from_order.mask_load_type, 
                from_order.mask_trailer_type)
            ) { // bad trailer or load type
                continue;
            }

            for(size_t k = 0; k < orders_count; ++k) {
                Order to_order = orders.GetOrder(k);
                
                int l = 0;
                int u = 1; 
                if(j==k) { // cant pick same order twice
                    u = 0;
                } else if (!IsExecutableBy(
                    truck.mask_load_type, 
                    truck.mask_trailer_type, 
                    to_order.mask_load_type, 
                    to_order.mask_trailer_type)
                ) { // bad trailer or load type
                    u = 0; 
                } else {
                    auto cost = data_.MoveBetweenOrders(from_order, to_order);
                    u &= cost.has_value(); // possible to arrive to k-th after j-th

                    // its not necessary for correctness to check if j-th order can be picked by i-th truck (it will be checked later anyway)
                    // but it will reduce amount of variables
                    u &= (truck.init_time <= from_order.start_time);
                }

                #ifdef NO_LONG_EDGES
                if (u != 0) {
                    double d = dists.GetDistance(from_order.to_city, to_order.from_city).value(); 
                    if(d > NO_LONG_EDGES) {
                        ++no_long_edges;
                        u = 0;
                    }
                }
                #endif
                
                if (u != 0) {
                    size_t ind = model.lp_.col_lower_.size();
                    to_index[{i,j,k}] = ind;
                    to_3d_variables_[ind] = {i,j,k};

                    model.lp_.col_lower_.push_back(l);
                    model.lp_.col_upper_.push_back(u);
                }
            }
        }
    }
    
    // lets add additional variables for later use
    // fake first order for each truck
    size_t fake_first_order = orders_count;
    static auto make_ffo = [] (const Truck& truck) -> Order {
        Order ffo;
        ffo.to_city = truck.init_city;
        ffo.finish_time = truck.init_time;
        return ffo;
    };

    static auto make_flo = [] (const Order& from_order) -> Order {
        Order flo;
        flo.from_city = from_order.to_city;
        flo.start_time = from_order.finish_time;
        return flo;
    };
    
    for(size_t i = 0; i < trucks_count; ++i) {
        Truck truck = trucks.GetTruck(i);

        // our fake first order (state after completing it <=> initial state of truck)
        Order from_order = make_ffo(truck);

        for(size_t k = 0; k < orders_count; ++k) {
            Order to_order = orders.GetOrder(k);
                
            int l = 0;
            int u = 1; 

            // we suppose to let any truck pick fake first order so we wont check any conditions for this order
            // but we have to check conditions for i-th truck and k-th order because it will be his real first order 
            if (!IsExecutableBy(
                truck.mask_load_type, 
                truck.mask_trailer_type, 
                to_order.mask_load_type, 
                to_order.mask_trailer_type)
            ) { // bad trailer or load type
                u = 0; 
            } else {
                auto cost = data_.MoveBetweenOrders(from_order, to_order);
                u &= cost.has_value(); // possible to complete j-th order as first one
            }

            if (u != 0) {
                size_t ind = model.lp_.col_lower_.size();
                to_index[{i,fake_first_order,k}] = ind;
                to_3d_variables_[ind] = {i,fake_first_order,k};
                model.lp_.col_lower_.push_back(l);
                model.lp_.col_upper_.push_back(u);
            }
        }
    }

    // fake last order for each truck
    size_t fake_last_order = orders_count + 1;
    for(size_t i = 0; i < trucks_count; ++i) {
        Truck truck = trucks.GetTruck(i);

        for(size_t j = 0; j < orders_count; ++j) {
            Order from_order = orders.GetOrder(j);

            // our fake last order (we suppose to make it pickable after any order)
            // we can set such configuration
            /*
            Order to_order = make_flo(from_order);
            */
            // but its more easier to just not check them
                
            int l = 0;
            int u = 1; 

            // we suppose to let any truck pick fake last order so we wont check any conditions for this order
            // but we have to check conditions for i-th truck and j-th order because it will be his real last order 
            if (!IsExecutableBy(
                truck.mask_load_type, 
                truck.mask_trailer_type, 
                from_order.mask_load_type, 
                from_order.mask_trailer_type)
            ) { // bad trailer or load type
                u = 0; 
            } else {
                // its not necessary for correctness to check if j-th order can be picked by i-th truck (it will be checked later anyway)
                // but it will reduce amount of variables
                u &= (truck.init_time <= from_order.start_time);
            }

            if (u != 0) {
                size_t ind = model.lp_.col_lower_.size();
                to_index[{i,j,fake_last_order}] = ind;
                to_3d_variables_[ind] = {i,j,fake_last_order};
                model.lp_.col_lower_.push_back(l);
                model.lp_.col_upper_.push_back(u);
            }
        }
    }

    // lets also let any truck to just pick only fake order <=> just not doing any real order
    for(size_t i = 0; i < trucks_count; ++i) {
        size_t ind = model.lp_.col_lower_.size();
        to_index[{i,fake_first_order,fake_last_order}] = ind;
        to_3d_variables_[ind] = {i,fake_first_order,fake_last_order};
        model.lp_.col_lower_.push_back(0);
        model.lp_.col_upper_.push_back(1);
    }

    #ifdef DEBUG_MODE
    cout << "##VARIABLES_DEBUG" << endl;
    for (auto &el : to_3d_variables_) {
        auto& [i,j,k] = el.second;
        std::string j_th_id = (j < orders_count 
            ? std::to_string(orders.GetOrder(j).order_id) 
            : 
            (j == fake_first_order 
                ? "ffo"
                : "flo"));
        
        std::string k_th_id = (k < orders_count 
            ? std::to_string(orders.GetOrder(k).order_id) 
            : 
            (k == fake_first_order 
                ? "ffo"
                : "flo"));        
        
        printf("[%2lu] {%2ld,%2ld,%2ld} for truck_id(%2d) edge (%4s, %4s)\n", el.first, i, j, k, trucks.GetTruck(i).truck_id, j_th_id.c_str(), k_th_id.c_str());
    }
    cout << "VARIABLES_DEBUG##" << endl;
    #endif

    #ifdef NO_LONG_EDGES
    cout << "[flag]no long edges: " << no_long_edges << " (out of " << n*m*m << ")" << endl;
    #endif
    #ifdef DEBUG_MODE
    cout << "non zero variables: " << to_3d_variables_.size() << " (out of " << trucks_count*(orders_count+1)*(orders_count+1) << ")" << endl;
    cout << "variables: " << to_3d_variables_.size() << endl;
    #endif

    // setting number of rows in l,u,x (number of variables)
    model.lp_.num_col_ = to_3d_variables_.size();

    // processing c
    // model.lp_.col_cost_
    model.lp_.col_cost_.resize(model.lp_.num_col_);
    for (auto &el : to_3d_variables_) {
        size_t ind = el.first;
        auto& [i,j,k] = el.second;
        Truck truck = trucks.GetTruck(i);
        
        double c = 0.;

        if (j != fake_first_order) {
            // real revenue from completing j-th order (revenue - time_cost - km_cost)
            c += data_.GetRealOrderRevenue(j);
        }

        Order from_order = (j != fake_first_order ? orders.GetOrder(j) : make_ffo(truck));
        Order to_order   = (k != fake_last_order  ? orders.GetOrder(k) : make_flo(from_order));

        // cost of moving to to_order.from_city and waiting until we can start it
        c += data_.MoveBetweenOrders(from_order, to_order).value();

        if (k == fake_last_order) {
            // taking in account weight of last city in scheduling scheme of this truck
            c += w_[from_order.to_city];
        }

        model.lp_.col_cost_[ind] = c;
    }
    #ifdef DEBUG_MODE
    cout << "##COST_VECTOR_DEBUG" << endl; 
    for (size_t ind = 0; ind < model.lp_.col_cost_.size(); ++ind) {
        auto& [i,j,k] = to_3d_variables_[ind];
        double c = model.lp_.col_cost_[ind];

        printf("C[{%2ld, %2ld, %2ld}] = %5f\n", i, j, k, c);
    }
    cout << "COST_VECTOR_DEBUG##" << endl; 
    #endif

    // A
    // L, U
    // model.lp_.a_matrix_.start_, model.lp_.a_matrix_.index_, model.lp_.a_matrix_.value_
    // model.lp_.row_lower_, model.lp_.row_upper_
    int number_of_rows = 0;
    int A_non_zeros = 0;

    #ifdef DEBUG_MODE
    auto A_matrix_element_debug = [this, &orders, &trucks, &number_of_rows, fake_first_order, fake_last_order, orders_count] (std::pair<size_t, int> &ind_val) {
        auto& [i,j,k] = to_3d_variables_[ind_val.first];
        std::string j_th_id = (j < orders_count 
            ? std::to_string(orders.GetOrder(j).order_id) 
            : 
            (j == fake_first_order 
                ? "ffo"
                : "flo"));
        
        std::string k_th_id = (k < orders_count 
            ? std::to_string(orders.GetOrder(k).order_id) 
            : 
            (k == fake_first_order 
                ? "ffo"
                : "flo"));        
        
        printf("A[%2d][{%2ld,%2ld,%2ld}] = %2d / truck_id(%2d) and edge(%4s, %4s)\n", 
            number_of_rows, i, j, k, ind_val.second, trucks.GetTruck(i).truck_id, j_th_id.c_str(), k_th_id.c_str());
    };
    auto LU_debug = [&number_of_rows] (int L, int U) {
        printf("L[%2d] = %2d, R[%2d] = %2d\n", number_of_rows, L, number_of_rows, U);
    };
    cout << "##INEQUALITIES_DEBUG (A matrix)" << endl;
    #endif

    // encoding condition 1: in sub-graph for i-th truck each vertex suppose to have equal incoming and outgoing degrees
    // fake vertexes act like source/drain so we shouldn put this constraint on them
    for(size_t i = 0; i < trucks_count; ++i) {
        for(size_t j = 0; j < orders_count; ++j) {
            std::vector<std::pair<size_t, int>> row_non_zeros;
            
            // k in [0,orders_count-1] -> real orders
            // k = orders_count -> fake first one
            // k = orders_count+1 -> fake last one
            for(size_t k = 0; k < orders_count + 1 + 1; ++k) {
                auto ind_from = GetVariableIndex({i,j,k});
                auto ind_to = GetVariableIndex({i,k,j});
                
                if (ind_from.has_value()) {
                    // A[this_row][{i,j,k}] += 1
                    row_non_zeros.emplace_back(ind_from.value(), 1);
                }
                if (ind_to.has_value()) {
                    // A[this_row][{i,k,j}] -= 1
                    row_non_zeros.emplace_back(ind_to.value(), -1);
                }
            }
            if (row_non_zeros.empty()) continue;
            // not empty row => +1 row in matrix A
            ++number_of_rows;

            // we check for i-th truck that all edges he picked satisfies condition1
            // L[this_row] <= A[this_row] * X <= R[this_row] 
            // <=> summary outgoind degree + -1 * (summary incoming degree) = 0 
            model.lp_.row_lower_.push_back(0);
            model.lp_.row_upper_.push_back(0);

            // setting number of non zero variables in current row of A matrix
            A_non_zeros += row_non_zeros.size();
            model.lp_.a_matrix_.start_.push_back(A_non_zeros);

            LU_debug(0,0);
            for(auto &el : row_non_zeros) {
                model.lp_.a_matrix_.index_.push_back(el.first);
                model.lp_.a_matrix_.value_.push_back(el.second);
                #ifdef DEBUG_MODE
                A_matrix_element_debug(el);
                #endif
            }

            #ifdef DEBUG_MODE
            cout<<"~~~~~~~~~~~~~~~~~~~~~~"<<endl;
            #endif
        }
    }
    
    // encoding condition 2: in all sub-graphs (for all trucks) each vertex suppose to have summary 0/1 outgoing degree
    // for obligation order it suppose to be 1
    for(size_t j = 0; j < orders_count; ++j) {
        std::vector<std::pair<size_t, int>> row_non_zeros;

        for(size_t i = 0; i < trucks_count; ++i) {
            // k in [0,orders_count-1] -> real orders
            // k = orders_count -> fake first one
            // k = orders_count+1 -> fake last one
            for(size_t k = 0; k < orders_count + 1 + 1; ++k) {
                // for fake last order we computing incoming degree
                auto ind_from = GetVariableIndex({i,j,k});
                
                if (ind_from.has_value()) {
                    // A[this_row][{i,j,k}] += 1
                    row_non_zeros.emplace_back(ind_from.value(), 1);
                }
            }
        }
        bool obligation = orders.GetOrder(j).obligation;
        if (row_non_zeros.empty()) {
            if(obligation) {
                std::cerr << "Error in scheduling obligation order_id("
                    << orders.GetOrder(j).order_id << "): no trucks can execute "
                    << j << "-th order" << endl;
                exit(1);
            }
        }
        // not empty row => +1 row in matrix A
        ++number_of_rows;

        model.lp_.row_lower_.push_back(obligation);
        model.lp_.row_upper_.push_back(1);

        // setting number of non zero variables in current row of A matrix
        A_non_zeros += row_non_zeros.size();
        model.lp_.a_matrix_.start_.push_back(A_non_zeros);

        LU_debug(model.lp_.row_lower_.back(), model.lp_.row_upper_.back());
        for(auto &el : row_non_zeros) {
            model.lp_.a_matrix_.index_.push_back(el.first);
            model.lp_.a_matrix_.value_.push_back(el.second);
            #ifdef DEBUG_MODE
            A_matrix_element_debug(el);
            #endif
        }

        #ifdef DEBUG_MODE
        cout<<"~~~~~~~~~~~~~~~~~~~~~~"<<endl;
        #endif
    }
    #ifdef DEBUG_MODE
    cout<<"FFO and FLO"<<endl;
    #endif
    // for fake first order summary outgoing degree suppose to be trucks_count (its source of the graph) 
    // but we will encode different constraint: in any given sub-graph it suppose to be 1 (its tighter one for our system)
    // fake last order - same stands for incoming degree
    for(size_t i = 0; i < trucks_count; ++i) {
        std::vector<std::pair<size_t, int>> row_non_zeros;

        // k in [0,orders_count-1] -> real orders
        // k = orders_count -> fake first one
        // k = orders_count+1 -> fake last one
        for(size_t k = 0; k < orders_count + 1 + 1; ++k) {
            // for fake last order we computing incoming degree
            auto ind_from = GetVariableIndex({i,fake_first_order,k});
            
            if (ind_from.has_value()) {
                // A[this_row][{i,j,k}] += 1
                row_non_zeros.emplace_back(ind_from.value(), 1);
            }
        }
        if (row_non_zeros.empty()) {
            continue;
        }
        // not empty row => +1 row in matrix A
        ++number_of_rows;

        model.lp_.row_lower_.push_back(1);
        model.lp_.row_upper_.push_back(1);

        // setting number of non zero variables in current row of A matrix
        A_non_zeros += row_non_zeros.size();
        model.lp_.a_matrix_.start_.push_back(A_non_zeros);

        LU_debug(model.lp_.row_lower_.back(), model.lp_.row_upper_.back());
        for(auto &el : row_non_zeros) {
            model.lp_.a_matrix_.index_.push_back(el.first);
            model.lp_.a_matrix_.value_.push_back(el.second);
            #ifdef DEBUG_MODE
            A_matrix_element_debug(el);
            #endif
        }

        #ifdef DEBUG_MODE
        cout<<"~~~~~~~~~~~~~~~~~~~~~~"<<endl;
        #endif
    }
    for(size_t i = 0; i < trucks_count; ++i) {
        std::vector<std::pair<size_t, int>> row_non_zeros;

        // j in [0,orders_count-1] -> real orders
        // j = orders_count -> fake first one
        for(size_t j = 0; j < orders_count + 1; ++j) {
            // for fake last order we computing incoming degree
            auto ind_from = GetVariableIndex({i,j,fake_last_order});
            
            if (ind_from.has_value()) {
                // A[this_row][{i,j,k}] += 1
                row_non_zeros.emplace_back(ind_from.value(), 1);
            }
        }
        if (row_non_zeros.empty()) {
            continue;
        }
        // not empty row => +1 row in matrix A
        ++number_of_rows;

        model.lp_.row_lower_.push_back(1);
        model.lp_.row_upper_.push_back(1);

        // setting number of non zero variables in current row of A matrix
        A_non_zeros += row_non_zeros.size();
        model.lp_.a_matrix_.start_.push_back(A_non_zeros);

        LU_debug(model.lp_.row_lower_.back(), model.lp_.row_upper_.back());
        for(auto &el : row_non_zeros) {
            model.lp_.a_matrix_.index_.push_back(el.first);
            model.lp_.a_matrix_.value_.push_back(el.second);
            #ifdef DEBUG_MODE
            A_matrix_element_debug(el);
            #endif
        }

        #ifdef DEBUG_MODE
        cout<<"~~~~~~~~~~~~~~~~~~~~~~"<<endl;
        #endif
    }

    #ifdef DEBUG_MODE
    cout << "##INEQUALITIES_DEBUG (A matrix)" << endl;
    #endif

    // number of rows in A,L,U
    model.lp_.num_row_ = number_of_rows;

    #ifdef DEBUG_MODE
    cout << "number of rows in A: " << number_of_rows << endl
        << "number of non zeros variables in A: " << A_non_zeros << endl;
    #endif
    
    return model;
}

solution_t WeightedCitiesSolver::Solve() {
    HighsModel model = CreateModel();
    
    Highs highs;
    HighsStatus return_status;
    
    return_status = highs.passModel(model);
    assert(return_status==HighsStatus::kOk);

    const HighsLp& lp = highs.getLp(); 

    return_status = highs.run();
    assert(return_status==HighsStatus::kOk);
    
    const HighsModelStatus& model_status = highs.getModelStatus();
    assert(model_status==HighsModelStatus::kOptimal);
    
    #ifdef DEBUG_MODE
    const HighsInfo& info = highs.getInfo();
    cout << "Model status: " << highs.modelStatusToString(model_status) << endl
        << "Simplex iteration count: " << info.simplex_iteration_count << endl
        << "Objective function value: " << info.objective_function_value << endl
        << "Primal  solution status: " << highs.solutionStatusToString(info.primal_solution_status) << endl
        << "Dual    solution status: " << highs.solutionStatusToString(info.dual_solution_status) << endl
        << "Basis: " << highs.basisValidityToString(info.basis_validity) << endl;
    #endif
    const bool has_values = info.primal_solution_status;
    const bool has_duals = info.dual_solution_status;
    const bool has_basis = info.basis_validity;
    
    const HighsSolution& solution = highs.getSolution();
    const HighsBasis& basis = highs.getBasis();
    
    model.lp_.integrality_.resize(lp.num_col_);
    for (int col=0; col < lp.num_col_; col++)
        model.lp_.integrality_[col] = HighsVarType::kInteger;

    highs.passModel(model);
    
    return_status = highs.run();
    assert(return_status==HighsStatus::kOk);
    
    #ifdef DEBUG_MODE
    cout << "##SOLVER_DEBUG" << endl;
    cout << "orders_count = " << data_.orders.Size() << endl;
    #endif

    std::vector<std::vector<int>> solution_arr(data_.trucks.Size(), std::vector<int>());
    size_t orders_count = data_.orders.Size();
    for (int col = 0; col < lp.num_col_; ++col) {
        if (info.primal_solution_status && solution.col_value[col]) {
            auto& [i,j,k] = to_3d_variables_[col];

            #ifdef DEBUG_MODE
            auto i_id = data_.trucks.GetTruck(i).truck_id;
            printf("{%2ld,%2ld,%2ld}", i, j, k);
            printf(" /// truck_id(%2d)", i_id);

            if (j < orders_count) {
                auto jth = data_.orders.GetOrder(j);
                printf("\n\t  /// from_order_id(%2d)[from_city(%2d),to_city(%2d)]", jth.order_id, jth.from_city, jth.to_city);
            }
            if (k < orders_count) {
                auto kth = data_.orders.GetOrder(k);
                printf("\n\t  /// to_order_id(%2d)[from_city(%2d),to_city(%2d)]", kth.order_id, kth.from_city, kth.to_city);
            }
            printf("\n");
            #endif

            if (0 < j && j < orders_count)
                solution_arr[i].push_back(j);
        }
    }

    #ifdef DEBUG_MODE
    cout << "SOLVER_DEBUG##" << endl;
    #endif

    return {solution_arr};
}
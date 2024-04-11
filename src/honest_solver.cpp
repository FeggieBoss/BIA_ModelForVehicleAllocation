#include "honest_solver.h"

// #define NO_LONG_EDGES

HonestSolver::HonestSolver() {}

void HonestSolver::SetData(const Data& data) {
    Solver::Solver::data_ = data;
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

HighsModel HonestSolver::CreateModel() {
    Params params = Solver::data_.params;
    Trucks trucks = Solver::data_.trucks;
    Orders orders = Solver::data_.orders;    
    Distances dists = Solver::data_.dists;

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
            const Order& from_order = orders.GetOrderConst(j);

            if (!IsExecutableBy(
                truck.mask_load_type, 
                truck.mask_trailer_type, 
                from_order.mask_load_type, 
                from_order.mask_trailer_type)
            ) { // bad trailer or load type
                continue;
            }

            for(size_t k = 0; k < orders_count; ++k) {
                const Order& to_order = orders.GetOrderConst(k);
                
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
                    auto cost = Solver::data_.MoveBetweenOrders(from_order, to_order);
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
                    Solver::to_3d_variables_[ind] = {i,j,k};

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
        const Truck& truck = trucks.GetTruckConst(i);

        // our fake first order (state after completing it <=> initial state of truck)
        Order from_order = make_ffo(truck);

        for(size_t k = 0; k < orders_count; ++k) {
            const Order& to_order = orders.GetOrderConst(k);
                
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
                auto cost = Solver::data_.MoveBetweenOrders(from_order, to_order);
                u &= cost.has_value(); // possible to complete j-th order as first one
            }

            if (u != 0) {
                size_t ind = model.lp_.col_lower_.size();
                to_index[{i,fake_first_order,k}] = ind;
                Solver::to_3d_variables_[ind] = {i,fake_first_order,k};
                model.lp_.col_lower_.push_back(l);
                model.lp_.col_upper_.push_back(u);
            }
        }
    }

    // fake last order for each truck
    size_t fake_last_order = orders_count + 1;
    for(size_t i = 0; i < trucks_count; ++i) {
        const Truck& truck = trucks.GetTruckConst(i);

        for(size_t j = 0; j < orders_count; ++j) {
            const Order& from_order = orders.GetOrderConst(j);

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
                Solver::to_3d_variables_[ind] = {i,j,fake_last_order};
                model.lp_.col_lower_.push_back(l);
                model.lp_.col_upper_.push_back(u);
            }
        }
    }

    // lets also let any truck to just pick only fake orders <=> simply not doing any real orders
    for(size_t i = 0; i < trucks_count; ++i) {
        size_t ind = model.lp_.col_lower_.size();
        to_index[{i,fake_first_order,fake_last_order}] = ind;
        Solver::to_3d_variables_[ind] = {i,fake_first_order,fake_last_order};
        model.lp_.col_lower_.push_back(0);
        model.lp_.col_upper_.push_back(1);
    }

    #ifdef DEBUG_MODE
    cout << "##VARIABLES_DEBUG" << endl;
    for (auto &el : Solver::to_3d_variables_) {
        auto& [i,j,k] = el.second;
        std::string j_th_id = (j < orders_count 
            ? std::to_string(orders.GetOrderConst(j).order_id) 
            : 
            (j == fake_first_order 
                ? "ffo"
                : "flo"));
        
        std::string k_th_id = (k < orders_count 
            ? std::to_string(orders.GetOrderConst(k).order_id) 
            : 
            (k == fake_first_order 
                ? "ffo"
                : "flo"));        
        
        printf("[%2lu] {%2ld,%2ld,%2ld} for truck_id(%2d) edge (%4s, %4s)\n", el.first, i, j, k, trucks.GetTruckConst(i).truck_id, j_th_id.c_str(), k_th_id.c_str());
    }
    cout << "VARIABLES_DEBUG##" << endl;
    #endif

    #ifdef NO_LONG_EDGES
    cout << "[flag]no long edges: " << no_long_edges << " (out of " << n*m*m << ")" << endl;
    #endif
    #ifdef DEBUG_MODE
    cout << "non zero variables: " << Solver::to_3d_variables_.size() << " (out of " << trucks_count*(orders_count+1)*(orders_count+1) << ")" << endl;
    cout << "variables: " << Solver::to_3d_variables_.size() << endl;
    #endif

    // setting number of rows in l,u,x (number of variables)
    model.lp_.num_col_ = Solver::to_3d_variables_.size();

    // processing c
    // model.lp_.col_cost_
    model.lp_.col_cost_.resize(model.lp_.num_col_);
    for (auto &el : Solver::to_3d_variables_) {
        size_t ind = el.first;
        auto& [i,j,k] = el.second;
        const Truck& truck = trucks.GetTruckConst(i);
        
        double c = 0.;

        if (j != fake_first_order) {
            // real revenue from completing j-th order (revenue - duty_time_cost - duty_km_cost)
            c += Solver::data_.GetRealOrderRevenue(j);
        }

        const Order& from_order = (j != fake_first_order ? orders.GetOrderConst(j) : make_ffo(truck));
        const Order& to_order   = (k != fake_last_order  ? orders.GetOrderConst(k) : make_flo(from_order));

        // cost of moving to to_order.from_city and waiting until we can start it
        c += Solver::data_.MoveBetweenOrders(from_order, to_order).value();

        model.lp_.col_cost_[ind] = c;
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

    // A
    // L, U
    // model.lp_.a_matrix_.start_, model.lp_.a_matrix_.index_, model.lp_.a_matrix_.value_
    // model.lp_.row_lower_, model.lp_.row_upper_
    int number_of_rows = 0;
    int A_non_zeros = 0;

    #ifdef DEBUG_MODE
    auto A_matrix_element_debug = [this, &orders, &trucks, &number_of_rows, fake_first_order, fake_last_order, orders_count] (std::pair<size_t, int> &ind_val) {
        auto& [i,j,k] = Solver::to_3d_variables_[ind_val.first];
        std::string j_th_id = (j < orders_count 
            ? std::to_string(orders.GetOrderConst(j).order_id) 
            : 
            (j == fake_first_order 
                ? "ffo"
                : "flo"));
        
        std::string k_th_id = (k < orders_count 
            ? std::to_string(orders.GetOrderConst(k).order_id) 
            : 
            (k == fake_first_order 
                ? "ffo"
                : "flo"));        
        
        printf("A[%2d][{%2ld,%2ld,%2ld}] = %2d / truck_id(%2d) and edge(%4s, %4s)\n", 
            number_of_rows, i, j, k, ind_val.second, trucks.GetTruckConst(i).truck_id, j_th_id.c_str(), k_th_id.c_str());
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

            #ifdef DEBUG_MODE
            LU_debug(0, 0);
            #endif

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
        bool obligation = orders.GetOrderConst(j).obligation;
        if (row_non_zeros.empty()) {
            if(obligation) {
                std::cerr << "Error in scheduling obligation order_id("
                    << orders.GetOrderConst(j).order_id << "): no trucks can execute "
                    << j << "-th order" << std::endl;
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

        #ifdef DEBUG_MODE
        LU_debug(model.lp_.row_lower_.back(), model.lp_.row_upper_.back());
        #endif
        
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

        #ifdef DEBUG_MODE
        LU_debug(model.lp_.row_lower_.back(), model.lp_.row_upper_.back());
        #endif
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

        #ifdef DEBUG_MODE
        LU_debug(model.lp_.row_lower_.back(), model.lp_.row_upper_.back());
        #endif

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
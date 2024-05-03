#include "honest_solver.h"

HonestSolver::HonestSolver() {}

void HonestSolver::SetData(const Data& data) {
    Solver::Solver::data_ = data;
}

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

    // variable_t -> its index in vector X
    std::vector<variable_t> variables;
    std::unordered_map<variable_t, size_t> to_index;
    auto GetVariableIndex = [&to_index] (const variable_t& v) -> std::optional<size_t> {
        auto it = to_index.find(v);
        if (it == to_index.end()) {
            return std::nullopt;
        }
        return it->second;
    };


    // processing l,u + storing non zero variables
    // model.lp_.col_lower_, model.lp_.col_upper_
    /*
        Note: for the sake of practicality and brevity: i = truck_pos, j = from_order_pos, k = to_order_pos

        lets calculate l,u for l <= x <= u
        for every (i,j,k) -> i-th truck will pick k-th order after completing j-th order
    */
    for (size_t truck_pos = 0; truck_pos < trucks_count; ++truck_pos) {
        Truck truck = trucks.GetTruck(truck_pos);

        for (size_t from_order_pos = 0; from_order_pos < orders_count; ++from_order_pos) {
            const Order& from_order = orders.GetOrderConst(from_order_pos);

            if (!IsExecutableBy(
                truck.mask_load_type, 
                truck.mask_trailer_type, 
                from_order.mask_load_type, 
                from_order.mask_trailer_type)
            ) { // bad trailer or load type
                continue;
            }

            for (size_t to_order_pos = 0; to_order_pos < orders_count; ++to_order_pos) {
                const Order& to_order = orders.GetOrderConst(to_order_pos);

                int u = 1; 
                if (from_order_pos == to_order_pos) { // cant pick same order twice
                    u = 0;
                } else {
                    auto cost = Solver::data_.MoveBetweenOrders(truck, from_order, to_order);
                    u &= cost.has_value(); // possible to arrive to k-th after j-th + k-th order is executable by i-th truck

                    // its not necessary for correctness to check if j-th order can be picked by i-th truck (it will be checked later anyway)
                    // but it will reduce amount of variables
                    u &= (truck.init_time <= from_order.start_time);
                }

                if (u != 0) {
                    variables.push_back({truck_pos, from_order_pos, to_order_pos});
                }
            }
        }
    }
    // lets add additional variables for later use
    // fake first order for each truck
    for (size_t truck_pos = 0; truck_pos < trucks_count; ++truck_pos) {
        const Truck& truck = trucks.GetTruckConst(truck_pos);

        // our fake first order (state after completing it <=> initial state of truck)
        Order from_order = Solver::make_ffo(truck);

        for (size_t to_order_pos = 0; to_order_pos < orders_count; ++to_order_pos) {
            const Order& to_order = orders.GetOrderConst(to_order_pos);
            int u = 1; 

            // we suppose to let any truck pick fake first order so we wont check any conditions for this order
            // but we have to check conditions for i-th truck and k-th order because it will be his real first order 
            auto cost = Solver::data_.MoveBetweenOrders(truck, from_order, to_order);
            u &= cost.has_value();

            if (u != 0) {
                variables.push_back({truck_pos, Solver::ffo_pos, to_order_pos});
            }
        }
    }

    // fake last order for each truck
    for (size_t truck_pos = 0; truck_pos < trucks_count; ++truck_pos) {
        const Truck& truck = trucks.GetTruckConst(truck_pos);

        for (size_t from_order_pos = 0; from_order_pos < orders_count; ++from_order_pos) {
            const Order& from_order = orders.GetOrderConst(from_order_pos);

            // our fake last order (we suppose to make it pickable after any order)
            // we can set such configuration
            /*
            Order to_order = Solver::make_flo(from_order);
            */
            // but its more easier to just not check them
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
                variables.push_back({truck_pos, from_order_pos, Solver::flo_pos});
            }
        }
    }

    // lets also let any truck to just pick only fake orders <=> simply not doing any real orders
    for (size_t truck_pos = 0; truck_pos < trucks_count; ++truck_pos) {
        variables.push_back({truck_pos, Solver::ffo_pos, Solver::flo_pos});
    }

    // #ifdef DEBUG_MODE
    std::cout << "variables (before PreSolve): " << variables.size() << std::endl;
    // #endif

    // filtering variables
    PreSolver pre_solver(variables);
    variables = pre_solver.GetFilteredVariables();
    
    // mapping variables to its indices in the model(indices of its columns) and vice versa
    for (size_t index = 0; index < variables.size(); ++index) {
        const variable_t& var = variables[index];

        to_index[var] = index;
        Solver::to_3d_variables_[index] = var;
        model.lp_.col_lower_.push_back(0);
        model.lp_.col_upper_.push_back(1);          
    }

    #ifdef DEBUG_MODE
    cout << "##VARIABLES_DEBUG" << endl;
    for (auto &el : Solver::to_3d_variables_) {
        const auto& [i,j,k] = el.second;
        std::string j_th_id = (j < orders_count 
            ? std::to_string(orders.GetOrderConst(j).order_id) 
            : 
            (j == Solver::ffo_pos 
                ? "ffo"
                : "flo"));
        
        std::string k_th_id = (k < orders_count 
            ? std::to_string(orders.GetOrderConst(k).order_id) 
            : 
            (k == Solver::ffo_pos 
                ? "ffo"
                : "flo"));        
        
        printf("[%2lu] {%2ld,%2ld,%2ld} for truck_id(%2d) edge (%4s, %4s)\n", el.first, i, j, k, trucks.GetTruckConst(i).truck_id, j_th_id.c_str(), k_th_id.c_str());
    }
    cout << "VARIABLES_DEBUG##" << endl;
    #endif

    #ifdef NO_LONG_EDGES
    cout << "[flag]no long edges: " << no_long_edges << " (out of " << n*m*m << ")" << endl;
    #endif
    // #ifdef DEBUG_MODE
    std::cout << "non zero variables: " << Solver::to_3d_variables_.size() << " (out of " << trucks_count*(orders_count+1)*(orders_count+1) << ")" << std::endl;
    // #endif

    // setting number of rows in l,u,x (number of variables)
    model.lp_.num_col_ = Solver::to_3d_variables_.size();

    // processing c
    // model.lp_.col_cost_
    model.lp_.col_cost_.resize(model.lp_.num_col_);
    for (auto &el : Solver::to_3d_variables_) {
        size_t ind = el.first;
        const auto& [truck_pos, from_order_pos, to_order_pos] = el.second;
        const Truck& truck = trucks.GetTruckConst(truck_pos);
        
        double c = 0.;

        if (from_order_pos != Solver::ffo_pos) {
            // real revenue from completing j-th order (revenue - duty_time_cost - duty_km_cost)
            c += Solver::data_.GetRealOrderRevenue(from_order_pos);
        }

        const Order& from_order = (from_order_pos != Solver::ffo_pos ? orders.GetOrderConst(from_order_pos) : Solver::make_ffo(truck));
        const Order& to_order   = (to_order_pos != Solver::flo_pos  ? orders.GetOrderConst(to_order_pos) : Solver::make_flo(from_order));

        // cost of moving to to_order.from_city and waiting until we can start it
        c += Solver::data_.CostMovingBetweenOrders(from_order, to_order).value();

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
    auto A_matrix_element_debug = [this, &orders, &trucks, &number_of_rows, orders_count] (std::pair<size_t, int> &ind_val) {
        const auto& [i,j,k] = Solver::to_3d_variables_[ind_val.first];
        std::string j_th_id = (j < orders_count 
            ? std::to_string(orders.GetOrderConst(j).order_id) 
            : 
            (j == Solver::ffo_pos 
                ? "ffo"
                : "flo"));
        
        std::string k_th_id = (k < orders_count 
            ? std::to_string(orders.GetOrderConst(k).order_id) 
            : 
            (k == Solver::ffo_pos 
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
    for (size_t truck_pos = 0; truck_pos < trucks_count; ++truck_pos) {
        for (size_t from_order_pos = 0; from_order_pos < orders_count; ++from_order_pos) {
            std::vector<std::pair<size_t, int>> row_non_zeros;
            
            // to_order_pos in [0,orders_count-1] -> real orders
            // to_order_pos = orders_count -> fake first one
            // to_order_pos = orders_count+1 -> fake last one
            for (size_t to_order_pos_i = 0; to_order_pos_i < orders_count + 1 + 1; ++to_order_pos_i) {
                size_t to_order_pos = to_order_pos_i;
                if (to_order_pos == orders_count) {
                    to_order_pos = Solver::ffo_pos;
                } else if (to_order_pos == orders_count + 1) {
                    to_order_pos = Solver::flo_pos;
                }

                auto ind_from = GetVariableIndex({truck_pos, from_order_pos, to_order_pos});
                auto ind_to = GetVariableIndex({truck_pos, to_order_pos, from_order_pos});
                
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

            for (auto &el : row_non_zeros) {
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
    for (size_t from_order_pos = 0; from_order_pos < orders_count; ++from_order_pos) {
        std::vector<std::pair<size_t, int>> row_non_zeros;

        for (size_t truck_pos = 0; truck_pos < trucks_count; ++truck_pos) {
            // to_order_pos in [0,orders_count-1] -> real orders
            // to_order_pos = orders_count -> fake first one
            // to_order_pos = orders_count+1 -> fake last one
            for (size_t to_order_pos_i = 0; to_order_pos_i < orders_count + 1 + 1; ++to_order_pos_i) {
                size_t to_order_pos = to_order_pos_i;
                if (to_order_pos == orders_count) {
                    to_order_pos = Solver::ffo_pos;
                } else if (to_order_pos == orders_count + 1) {
                    to_order_pos = Solver::flo_pos;
                }

                // for fake last order we computing incoming degree
                auto ind_from = GetVariableIndex({truck_pos, from_order_pos, to_order_pos});
                
                if (ind_from.has_value()) {
                    // A[this_row][{i,j,k}] += 1
                    row_non_zeros.emplace_back(ind_from.value(), 1);
                }
            }
        }
        bool obligation = orders.GetOrderConst(from_order_pos).obligation;
        if (row_non_zeros.empty()) {
            if (obligation) {
                std::cerr << "Error in scheduling obligation order_id("
                    << orders.GetOrderConst(from_order_pos).order_id << "): no trucks can execute "
                    << from_order_pos << "-th order" << std::endl;
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
        
        for (auto &el : row_non_zeros) {
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
    for (size_t truck_pos = 0; truck_pos < trucks_count; ++truck_pos) {
        std::vector<std::pair<size_t, int>> row_non_zeros;

        // to_order_pos in [0,orders_count-1] -> real orders
        // to_order_pos = orders_count -> fake first one
        // to_order_pos = orders_count+1 -> fake last one
        for (size_t to_order_pos_i = 0; to_order_pos_i < orders_count + 1 + 1; ++to_order_pos_i) {
            size_t to_order_pos = to_order_pos_i;
            if (to_order_pos == orders_count) {
                to_order_pos = Solver::ffo_pos;
            } else if (to_order_pos == orders_count + 1) {
                to_order_pos = Solver::flo_pos;
            }

            // for fake first order we computing outcoming degree
            auto ind_from = GetVariableIndex({truck_pos, Solver::ffo_pos, to_order_pos});
            
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
        for (auto &el : row_non_zeros) {
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
    for (size_t truck_pos = 0; truck_pos < trucks_count; ++truck_pos) {
        std::vector<std::pair<size_t, int>> row_non_zeros;

        // from_order_pos in [0,orders_count-1] -> real orders
        // from_order_pos = orders_count -> fake first one
        for (size_t from_order_pos_i = 0; from_order_pos_i < orders_count + 1; ++from_order_pos_i) {
            size_t from_order_pos = from_order_pos_i;
            if (from_order_pos == orders_count) {
                from_order_pos = Solver::ffo_pos;
            }

            // for fake last order we computing incoming degree
            auto ind_from = GetVariableIndex({truck_pos, from_order_pos, Solver::flo_pos});
            
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

        for (auto &el : row_non_zeros) {
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
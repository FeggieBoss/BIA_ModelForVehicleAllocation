#include "chain_solver.h"

ChainSolver::ChainSolver(double min_chain_revenue, size_t mx_chain_len) :
    min_chain_revenue_(min_chain_revenue),
    chain_generator(min_chain_revenue_, mx_chain_len)
{};


const Data& ChainSolver::GetDataConst() const {
    return data_;
}

void ChainSolver::SetData(const Data& data) {
    data_ = data;
    to_2d_variables = {};
    real_orders_count = data_.orders.Size();
    chain_generator.GenerateChains(data_);
}

void ChainSolver::SetData(const Data& data, const FreeMovementWeightsVectors& edges_w_vecs) {
    SetData(data);
    chain_generator.AddWeightsEdges(data_, edges_w_vecs);
}

HighsModel ChainSolver::CreateModel() {
    Trucks trucks = data_.trucks;
    Orders orders = data_.orders;

    size_t trucks_count = trucks.Size();
    size_t orders_count = orders.Size();

    const std::vector<std::vector<Chain>>& chains_by_truck_pos = chain_generator.chains_by_truck_pos;

    // c^Tx + d subject to L <= Ax <= U; l <= x <= u
    HighsModel model;
    // maximizing revenue => kMaximize
    model.lp_.sense_ = ObjSense::kMaximize;
    // writing matrix A by rows
    model.lp_.a_matrix_.format_ = MatrixFormat::kRowwise;
    model.lp_.offset_ = 0;


    #ifdef DEBUG_MODE
    cout << "##CHAIN_SOLVER_DEBUG" << endl;
    #endif

    // processing l,u + storing non zero variables
    // model.lp_.col_lower_, model.lp_.col_upper_
    /*
        Note: for the sake of practicality and brevity: i = truck_pos, c = local_chain_pos
        (also chain_pos in all loops will actually mean local_chain_pos by default)

        lets calculate l,u for l <= x <= u
        for every (i,c) -> i-th truck will pick c-th chain in its set of chains
    */
    std::unordered_map<std::tuple<size_t, size_t>, size_t> to_index;
    size_t cur_var_id = 0;
    for (size_t truck_pos = 0; truck_pos < trucks_count; ++truck_pos) {
        const auto& chains = chains_by_truck_pos[truck_pos];
        const size_t chains_count = chains.size();
        
        for (size_t chain_pos = 0; chain_pos < chains_count; ++chain_pos) {
            to_2d_variables[cur_var_id] = chain_variable_t{truck_pos, chain_pos};
            to_index[{truck_pos, chain_pos}] = cur_var_id++;

            model.lp_.col_lower_.push_back(0);
            model.lp_.col_upper_.push_back(1);
        }
    }


    // setting number of rows in l,u,x (number of variables)
    model.lp_.num_col_ = to_2d_variables.size();


    // processing c
    // model.lp_.col_cost_
    model.lp_.col_cost_.resize(model.lp_.num_col_);
    for (auto &el : to_2d_variables) {
        size_t ind = el.first;
        const auto& [truck_pos, chain_pos] = el.second;
        
        double c = chains_by_truck_pos[truck_pos][chain_pos].revenue;
        model.lp_.col_cost_[ind] = c;
    }


    // A
    // L, U
    // model.lp_.a_matrix_.start_, model.lp_.a_matrix_.index_, model.lp_.a_matrix_.value_
    // model.lp_.row_lower_, model.lp_.row_upper_
    int number_of_rows = 0;
    int A_non_zeros = 0;

    /*
        encoding condition 1 (from assignment problem): 
        each truck suppose to take no more than one chain
    */
    for (size_t truck_pos = 0; truck_pos < trucks_count; ++truck_pos) {
        const auto& chains = chains_by_truck_pos[truck_pos];
        const size_t chains_count = chains.size();
        
        if (chains_count == 0) continue;
        // not empty row => +1 row in matrix A
        ++number_of_rows;
        
        // L[this_row] <= A[this_row] * X <= R[this_row] 
        // we want sum of picked chains to be 0 or 1
        model.lp_.row_lower_.push_back(0);
        model.lp_.row_upper_.push_back(1);

        // setting number of non zero variables in current row of A matrix
        A_non_zeros += chains_count;
        model.lp_.a_matrix_.start_.push_back(A_non_zeros);

        // creating row in A matrix
        for (size_t chain_pos = 0; chain_pos < chains_count; ++chain_pos) {
            // A[this_row][{i,c}] = 1
            model.lp_.a_matrix_.index_.push_back(to_index[{truck_pos, chain_pos}]);
            model.lp_.a_matrix_.value_.push_back(1);
        }
    }

    /*
        condition ? (from assignment problem):
        each chain suppose to be picked by no more than one truck
        But actually we can really see that we made our set of variables in such way that its provided by default
        (sets of trucks chains dont intersect) 
    */

    /*
        condition 2:
        picked chains must not intersect by orders that belong to this chains
        (intersection of chains {0, 1} and {1, 2} is {1} for example)
    */
    // for order with order_pos provides set of indexes of variables {i, c} such that order belong to chain 'c'   
    std::vector<std::vector<size_t>> vars_by_order_pos(orders_count);
    for (size_t truck_pos = 0; truck_pos < trucks_count; ++truck_pos) {
        const auto& chains = chains_by_truck_pos[truck_pos];
        const size_t chains_count = chains.size();
        
        for (size_t chain_pos = 0; chain_pos < chains_count; ++chain_pos) {
            const Chain& chain = chains[chain_pos];
            size_t chain_len = chain.GetEndPos();

            for (size_t i = 0; i < chain_len; ++i) {
                size_t order_pos = chain[i];

                vars_by_order_pos[order_pos].push_back(to_index[{truck_pos, chain_pos}]);
            }
        }
    }

    for (size_t order_pos = 0; order_pos < orders_count; ++order_pos) {
        const size_t vars_count = vars_by_order_pos[order_pos].size();
        if (vars_count == 0) continue;

        // not empty row => +1 row in matrix A
        ++number_of_rows;
        
        // L[this_row] <= A[this_row] * X <= R[this_row] 
        /*
            we want sum of picked chains that contains order with 'order_pos' to be 0 or 1
            Note: if order with order_pos has obligation = 1 we can make at least one truck pick it by setting L[this_row] = 1
        */
        model.lp_.row_lower_.push_back(orders.GetOrderConst(order_pos).obligation);
        model.lp_.row_upper_.push_back(1);

        // setting number of non zero variables in current row of A matrix
        A_non_zeros += vars_count;
        model.lp_.a_matrix_.start_.push_back(A_non_zeros);

        // creating row in A matrix
        for (size_t var_index : vars_by_order_pos[order_pos]) {
            // A[this_row][var_index] = 1
            model.lp_.a_matrix_.index_.push_back(var_index);
            model.lp_.a_matrix_.value_.push_back(1);
        }
    }

    // number of rows in A,L,U
    model.lp_.num_row_ = number_of_rows;

    #ifdef DEBUG_MODE
    cout << "number of rows in A: " << number_of_rows << endl
        << "number of non zeros variables in A: " << A_non_zeros << endl;
    #endif

    #ifdef DEBUG_MODE
    cout << "CHAIN_SOLVER_DEBUG##" << endl;
    #endif
    return model;
}

solution_t ChainSolver::Solve() {
    auto model = CreateModel();
    std::vector<size_t> setted_columns = Solver::Solve(model);

    std::vector<std::vector<size_t>> orders_by_truck_pos(data_.trucks.Size(), std::vector<size_t>());
    size_t orders_count = data_.orders.Size();

    #ifdef DEBUG_MODE
    cout << "##CHAIN_SOLVER_DEBUG" << endl;
    cout << "orders_count = " << orders_count << endl;
    #endif

    for(size_t var : setted_columns) {
        auto& [truck_pos, chain_pos] = to_2d_variables[var];
        
        const std::vector<std::vector<Chain>>& chains_by_truck_pos = chain_generator.chains_by_truck_pos;
        const Chain& chain = chains_by_truck_pos[truck_pos][chain_pos];

        size_t chain_len = chain.GetEndPos();
        for (size_t i = 0; i < chain_len; ++i) {
            size_t order_pos = chain[i];

            orders_by_truck_pos[truck_pos].push_back(order_pos);
        }
    }

    #ifdef DEBUG_MODE
    cout << "CHAIN_SOLVER_DEBUG##" << endl;
    #endif
    return {orders_by_truck_pos};
}
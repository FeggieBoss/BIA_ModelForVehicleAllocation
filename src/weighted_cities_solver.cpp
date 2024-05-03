#include "weighted_cities_solver.h"

//////////////////////////
// WeightedCitiesSolver //
//////////////////////////

WeightedCitiesSolver::WeightedCitiesSolver() {}

const Data& WeightedCitiesSolver::GetDataConst() const {
    return Solver::data_;
}

void WeightedCitiesSolver::ModifyData(Data& data) const {
    auto [additional_orders, _] = edges_w_vecs_.GetFreeMovementEdges(data);
    for (const Order& order : additional_orders) {
        data.orders.AddOrder(order);
    }

    #ifdef DEBUG_MODE
    cout << "ADDED FREE-MOVEMENT-ORDERS:"<<endl;
    data.orders.DebugPrint();
    #endif
}

void WeightedCitiesSolver::SetData(const Data& data) {
    Solver::data_ = data;
    ModifyData(Solver::data_);
}

void WeightedCitiesSolver::SetData(const Data& data, unsigned int t, const FreeMovementWeightsVectors& edges_w_vecs) {
    edges_w_vecs_ = edges_w_vecs;
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
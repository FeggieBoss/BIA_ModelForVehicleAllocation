#include "solver.h"

size_t Solver::ffo_pos = (size_t)(-1);
size_t Solver::flo_pos = (size_t)(-2);
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

solution_t Solver::Solve() {
    HighsModel model = CreateModel();
    
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
    
    #ifdef DEBUG_MODE
    cout << "##SOLVER_DEBUG" << endl;
    cout << "orders_count = " << data_.orders.Size() << endl;
    #endif

    std::vector<std::vector<size_t>> orders_by_truck_pos(data_.trucks.Size(), std::vector<size_t>());
    size_t orders_count = data_.orders.Size();
    for (int col = 0; col < lp.num_col_; ++col) {
        if (info.primal_solution_status && solution.col_value[col]) {
            auto& [i, j, k] = to_3d_variables_[col];

            #ifdef DEBUG_MODE
            auto i_id = data_.trucks.GetTruckConst(i).truck_id;
            printf("{%2ld,%2ld,%2ld}", i, j, k);
            printf(" /// truck_id(%2d)", i_id);

            if (j < orders_count) {
                auto jth = data_.orders.GetOrderConst(j);
                printf("\n\t   /// from_order_id(%2d)[from_city(%2d),to_city(%2d)]", jth.order_id, jth.from_city, jth.to_city);
            } else {
                std::string name = (j == Solver::ffo_pos ? "ffo" : "flo");
                printf("\n\t   /// %s", name.c_str());
            }
            if (k < orders_count) {
                auto kth = data_.orders.GetOrderConst(k);
                printf("\n\t   /// to_order_id(%2d)[from_city(%2d),to_city(%2d)]", kth.order_id, kth.from_city, kth.to_city);
            } else {
                std::string name = (k == Solver::ffo_pos ? "ffo" : "flo");
                printf("\n\t   /// %s", name.c_str());
            }
            printf("\n");
            #endif

            // not adding fake orders in solution
            if (j < orders_count)
                orders_by_truck_pos[i].push_back(j);
        }
    }

    size_t trucks_count = data_.trucks.Size();
    const Orders& orders = data_.orders;
    for(size_t truck_pos = 0; truck_pos < trucks_count; ++truck_pos) {
        auto &scheduled_orders = orders_by_truck_pos[truck_pos];

        // organizing scheduled orders in the order they will be completed
        sort(scheduled_orders.begin(), scheduled_orders.end(), [&orders](const int& a, const int& b) {
            return orders.GetOrderConst(a).start_time < orders.GetOrderConst(b).start_time;
        });
    }

    #ifdef DEBUG_MODE
    cout << "SOLVER_DEBUG##" << endl;
    #endif

    return {orders_by_truck_pos};
}
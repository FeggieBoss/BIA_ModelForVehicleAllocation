#include "solver.h"

solution_t Solver::Solve() {
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
    
    const HighsInfo& info = highs.getInfo();
    #ifdef DEBUG_MODE
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

    std::vector<std::vector<size_t>> solution_arr(data_.trucks.Size(), std::vector<size_t>());
    size_t orders_count = data_.orders.Size();
    for (int col = 0; col < lp.num_col_; ++col) {
        if (info.primal_solution_status && solution.col_value[col]) {
            auto& [i,j,k] = to_3d_variables_[col];

            #ifdef DEBUG_MODE
            auto i_id = data_.trucks.GetTruckConst(i).truck_id;
            printf("{%2ld,%2ld,%2ld}", i, j, k);
            printf(" /// truck_id(%2d)", i_id);

            if (j < orders_count) {
                auto jth = data_.orders.GetOrderConst(j);
                printf("\n\t   /// from_order_id(%2d)[from_city(%2d),to_city(%2d)]", jth.order_id, jth.from_city, jth.to_city);
            } else {
                std::string name = (j == orders_count ? "ffo" : "flo");
                printf("\n\t   /// %s", name.c_str());
            }
            if (k < orders_count) {
                auto kth = data_.orders.GetOrderConst(k);
                printf("\n\t   /// to_order_id(%2d)[from_city(%2d),to_city(%2d)]", kth.order_id, kth.from_city, kth.to_city);
            } else {
                std::string name = (k == orders_count ? "ffo" : "flo");
                printf("\n\t   /// %s", name.c_str());
            }
            printf("\n");
            #endif

            // not adding fake orders in solution
            if (j < orders_count)
                solution_arr[i].push_back(j);
        }
    }

    #ifdef DEBUG_MODE
    cout << "SOLVER_DEBUG##" << endl;
    #endif

    return {solution_arr};
}
#ifndef DEFINE_WEIGHTED_CITIES_SOLVER_H
#define DEFINE_WEIGHTED_CITIES_SOLVER_H

#include "flow_solver.h"

/*
    !!!Note!!!: parents method "solution_t Solve()" will produce unexpected solution
    Solution will be generated according to modified Data (Data after ModifyData method)
    But ModifyData provides such constraints which helps to work with such solution:
    (1) Free-movement orders always being added right after all real orders
    (2) Free-movement orders always have order_id = 0
    There is two ways of working with such solution:
    1. We dont want to use free-movement orders:
        we should just use our unmodified Data
        and when we encounter orders | order_pos >= data.orders.Size() we should just ignore such orders
        (we are using (1) for correctness here)
    2. We want to use free-movement orders:
        we should get Data with free-movement orders in it (Data after 'ModifyData') by GetDataConst method
        and then simply using it as our Data
        (we can distinguish free-movement order because all of them have 'order_id = 0' as (2) claims)
*/
class WeightedCitiesSolver : public Solver {
private:
    Data data_;

    // if truck choose to move without order to another city we set limit of time he has to do so
    std::optional<unsigned int> time_boundary_ = std::nullopt;
    // weights of last cities in trucks schedules
    FreeMovementWeightsVectors edges_w_vecs_;
    FlowSolver flow_solver;

    // Adding new free-movement edges according to FreeMovementWeightsVectors
    void ModifyData(Data& data) const;
public:
    WeightedCitiesSolver();
    
    const Data& GetDataConst() const override;

    /*
        No time boundary means no fine for waiting after completing last order
        With FreeMovementWeightsVectors = 0 will produce same solution as HonestSolver
        !!!Note!!!:
        (1) ModifyData being called
        (2) FreeMovementWeightsVectors will be all zeros (if u want non zero one call other method)
    */
    void SetData(const Data& data) override;

    /*
        Adding fine for waiting after completing last order until time_boundary
        !!!Note!!!:
        (1) ModifyData being called
        (2) we expect for all orders that order.start_time < time_boundary
    */
    void SetData(const Data& data, unsigned int time_boundary, const FreeMovementWeightsVectors& edges_w_vecs);
    
    HighsModel CreateModel() override;
    solution_t Solve() override;
};

#endif // DEFINE_WEIGHTED_CITIES_SOLVER_H
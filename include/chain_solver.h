#ifndef DEFINE_ASSIGNMENT_SOLVER_H
#define DEFINE_ASSIGNMENT_SOLVER_H

#include "chain_generator.h"


/*  
    {truck_pos, local_chain_pos} 
    where local_chain_pos - chain position in set of all chains for truck with 'truck_pos'
*/
typedef std::pair<size_t, size_t> chain_variable_t;

class ChainSolver : public Solver {
private:
    // we want to understand either we work with real order or with free-movement one
    size_t real_orders_count;

    Data data_;
    double min_chain_revenue_;
    ChainGenerator chain_generator;

    // index of variable in vector X -> {truck_pos, local_chain_pos}
    std::unordered_map<size_t, chain_variable_t> to_2d_variables;

public:
    ChainSolver(double min_chain_revenue, size_t mx_chain_len);

    void SetData(const Data& data) override;
    
    /*
        Note: we need to add free-movement edges to set of orders
        ChainSolver provides GetDataConst to get modified data (data with new set of orders) if needed
        (Notes from WeightedCitiesSolver can be useful to understand how to work with modified data) 
    */
    void SetData(const Data& data, const FreeMovementWeightsVectors& edges_w_vecs);
    const Data& GetDataConst() const override;

    HighsModel CreateModel() override;
    solution_t Solve() override;
};

#endif // DEFINE_ASSIGNMENT_SOLVER_H
#ifndef DEFINE_PRE_SOLVER_H
#define DEFINE_PRE_SOLVER_H

#include "solver.h"

struct PreSolverNode {
    size_t to;
    size_t var_index;
};

/* 
    Filtering variables of the model
    Note: variables must form oriented acyclic(not even cycles of length 2 allowed) grapgh
*/ 
class PreSolver {
public:
    PreSolver(const std::vector<variable_t>& variables);
    std::vector<variable_t> GetFilteredVariables() const;

private:
    size_t GetGraphNode(size_t order_pos) const;
    void MarkConnectivityComponent(size_t v);
    void ClearGraph();
    void PreSolve();

    // variable by its index
    std::vector<variable_t> variables_;
    size_t trucks_count_ = 0;
    size_t orders_count_ = 0;
    /*
        storing if variable should stay after filtering by its index <=> 2
        because we add +1 in MarkConnectivityComponent(source)
                       +1 time in MarkConnectivityComponent(sink)
    */
    std::vector<int> valid_vars_;
    // sub-graph for some truck
    std::vector<std::vector<PreSolverNode>> graph_;
    std::vector<bool> used_;
};

#endif // DEFINE_PRE_SOLVER_H
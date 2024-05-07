#ifndef DEFINE_FLOW_SOLVER_H
#define DEFINE_FLOW_SOLVER_H

#include "pre_solver.h"

class FlowSolver : public Solver {
private:
    Data data_;

// TO DO: make protected
public:
    // index of variable in vector X -> its 3d/variable_t representation
    std::unordered_map<size_t, variable_t> to_3d_variables;

public:

    FlowSolver();
    
    void SetData(const Data& data) override;
    HighsModel CreateModel() override;
    solution_t Solve() override;
    const Data& GetDataConst() const override;

    solution_t Solve(HighsModel& model);
};

#endif // DEFINE_FLOW_SOLVER_H
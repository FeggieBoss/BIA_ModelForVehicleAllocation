#ifndef DEFINE_HONEST_SOLVER_H
#define DEFINE_HONEST_SOLVER_H

#include "main.h"
#include "solver.h"

class HonestSolver : public Solver {
private:
    Data data_;
    // index of variable in vector X -> its 3d/variable_t representation
    std::unordered_map<size_t, variable_t> to_3d_variables_;

    HighsModel CreateModel();
public:
    HonestSolver();
    
    void SetData(const Data& data) override;
    solution_t Solve() override;
};

#endif // DEFINE_HONEST_SOLVER_H
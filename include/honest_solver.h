#ifndef DEFINE_HONEST_SOLVER_H
#define DEFINE_HONEST_SOLVER_H

#include "solver.h"

class HonestSolver : public Solver {
public:
    HonestSolver();
    
    void SetData(const Data& data) override;
    HighsModel CreateModel() override;
};

#endif // DEFINE_HONEST_SOLVER_H
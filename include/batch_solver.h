#ifndef DEFINE_BATCH_SOLVER_H
#define DEFINE_BATCH_SOLVER_H

#include "weighted_cities_solver.h"

class BatchSolver {
public:
    BatchSolver();
    solution_t Solve(const Data& data, unsigned int time_window);
};

#endif // DEFINE_BATCH_SOLVER_H
#ifndef DEFINE_BATCH_SOLVER_H
#define DEFINE_BATCH_SOLVER_H

#include "weighted_cities_solver.h"
#include "chain_solver.h"

#include <set>

enum class SOLVER_MODEL_TYPE {
    FLOW_MODEL,
    ASSIGNMENT_MODEL
};

class BatchSolver {
private:
    SOLVER_MODEL_TYPE solver_model_type_;
    std::shared_ptr<Solver> solver_;

public:
    BatchSolver(std::shared_ptr<WeightedCitiesSolver> solver);
    BatchSolver(std::shared_ptr<ChainSolver> solver);
    solution_t Solve(const Data& data, unsigned int time_window);
};

#endif // DEFINE_BATCH_SOLVER_H
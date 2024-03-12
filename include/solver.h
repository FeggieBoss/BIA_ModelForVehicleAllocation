#ifndef DEFINE_SOLVER_H
#define DEFINE_SOLVER_H

#include "Highs.h"
#include "solution.h"
#include "data.h"

class Solver {
public:
    Solver() = default;
    virtual void SetData(const Data& data) = 0;
    virtual solution_t Solve() = 0;
};

#endif // DEFINE_SOLVER_H

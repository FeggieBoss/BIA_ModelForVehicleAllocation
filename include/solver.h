#ifndef DEFINE_SOLVER_H
#define DEFINE_SOLVER_H

#include "Highs.h"
#include "solution.h"
#include "data.h"

// {truck, from_order, to_order}
typedef std::tuple<size_t, size_t, size_t> variable_t;

class Solver {
public:
    Solver() = default;
    virtual void SetData(const Data& data) = 0;
    virtual solution_t Solve() = 0;
};

#endif // DEFINE_SOLVER_H

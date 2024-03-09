#ifndef DEFINE_CHECKER_H
#define DEFINE_CHECKER_H

#include "data.h"
#include "solution.h"
#include "main.h"

#include <optional>
#include <set>
#include <cassert>

class Checker {
private:
    Data data_;
    solution_t solution_;
public:
    Checker() = default;
    Checker(const Data& data); 

    void SetSolution(const solution_t& solution);
    std::optional<double> Check();
};

#endif // DEFINE_CHECKER_H
#include "main.h"

#include "checker.h"
#include "honest_solver.h"
#include "weighted_cities_solver.h"

#include <cassert>

int main() {
    std::string params_path = "./../case_1/params_small.xlsx";
    std::string trucks_path = "./../case_1/trucks_small.xlsx";
    std::string orders_path = "./../case_1/orders_small.xlsx";
    std::string dists_path = "./../case_1/distances_small.xlsx";
    
    Data data(params_path, trucks_path, orders_path, dists_path);
    data.ShiftTimestamps();

    // data.params.DebugPrint();
    // data.trucks.DebugPrint();
    // data.orders.DebugPrint();
    // data.dists.DebugPrint();

    // solution_t solution = {
    //     {
    //         {0, 1, 3},
    //         {2}
    //     }
    // };

    // weights for 6 cities
    weights_vector_t w = {
        0., 
        0., // 1
        0., // 2
        0., // 3
        0., // 4
        0., // 5
        0.  // 6
    };

    // HonestSolver solver;
    // solver.SetData(data);
    // solution_t solution = solver.Solve();

    WeightedCitiesSolver solver;
    // asking WSolver to add free movement edges
    solver.ModifyData(data);
    solver.SetData(data, 150);
    solver.SetVectorW(w);
    solution_t solution = solver.Solve();

    Checker checker(data);
    checker.SetSolution(solution);
    assert(checker.Check().has_value());
    return 0;
}
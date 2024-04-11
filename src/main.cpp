#include "main.h"

#include "checker.h"
#include "honest_solver.h"
#include "weighted_cities_solver.h"
#include "batch_solver.h"

#include <cassert>

int main() {
    std::string params_path = "./../samples/params_small.xlsx";
    std::string trucks_path = "./../samples/trucks_small.xlsx";
    std::string orders_path = "./../samples/orders_small.xlsx";
    std::string dists_path = "./../samples/distances_small.xlsx";
    
    Data data(params_path, trucks_path, orders_path, dists_path);

    data.ShiftTimestamps();
    data.SqueezeCitiesIds();

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

    // HonestSolver solver;
    // solver.SetData(data);
    // solution_t solution = solver.Solve();

    // WeightedCitiesSolver solver;
    // // asking WSolver to add free movement edges
    // solver.ModifyData(data);
    // solver.SetData(data); // (data, 150)
    // solver.SetVectorW(w);
    // solution_t solution = solver.Solve();

    for (Order &order : data.orders) {
        order.obligation = 0;
    }   

    BatchSolver solver;
    solution_t solution = solver.Solve(data, 24*60);


    Checker checker(data);
    checker.SetSolution(solution);
    assert(checker.Check().has_value());
    return 0;
}
#include "main.h"

#include "checker.h"

#include <cassert>

using std::cout;
using std::endl;

int main() {
    std::string params_path = "./../case_1/params_small.xlsx";
    std::string trucks_path = "./../case_1/trucks_small.xlsx";
    std::string orders_path = "./../case_1/orders_small.xlsx";
    std::string dists_path = "./../case_1/distances_small.xlsx";
    
    Data data(params_path, trucks_path, orders_path, dists_path);
    data.ShiftTimestamps();

    data.params.DebugPrint();
    data.trucks.DebugPrint();
    data.orders.DebugPrint();
    data.dists.DebugPrint();

    solution_t solution = {
        {
            {0, 1, 3},
            {2}
        }
    };

    Checker checker(data);
    checker.SetSolution(solution);
    assert(checker.Check().has_value());

    //parse_input_data(data);
    

    // auto model = create_model(data);

    // auto setted_variables = solve(model.model);

    // std::vector<std::vector<int>> ans(data.n);
    // for(int c : setted_variables) {
    //     int it = find_if(model.variables.begin(), model.variables.end(), [c](const variable_t &v) {
    //         return v.column == c;
    //     }) - model.variables.begin();
    //     const auto [_,i,j,k] = model.variables[it];
    //     cout<<"truck("<<i<<"): from("<<j<<") "<<"to ("<<k<<")"<<endl;
    //     ans[i-1].push_back(j);
    // }

    // assert(checker(ans, data.trucks, data.orders, data.dists)==true);

    return 0;
}
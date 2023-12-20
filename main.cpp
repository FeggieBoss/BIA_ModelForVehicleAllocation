#include "main.h"
#include "parse.h"
#include "model.h"

#include <cassert>

using std::cout;
using std::endl;

data_t data;

int main() {
    parse(data);

    auto model = create_model(data);

    auto setted_variables = solve(model.model);

    std::vector<std::vector<int>> ans(data.n);
    for(int c : setted_variables) {
        int it = find_if(model.variables.begin(), model.variables.end(), [c](const variable_t &v) {
            return v.column == c;
        }) - model.variables.begin();
        const auto [_,i,j,k] = model.variables[it];
        cout<<"truck("<<i<<"): from("<<j<<") "<<"to ("<<k<<")"<<endl;
        ans[i-1].push_back(j);
    }

    assert(checker(ans, data.trucks, data.orders, data.dists)==true);

    return 0;
}
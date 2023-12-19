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

    auto ans = solve(model);

    assert(checker(ans, data.trucks, data.orders, data.dists)==true);

    return 0;
}
#include "main.h"

#include "checker.h"
#include "honest_solver.h"
#include "weighted_cities_solver.h"
#include "batch_solver.h"

#include <cassert>

#include <iomanip>

void filter_trucks(size_t count, unsigned int time_bound, std::vector<Truck>& filtered_trucks, Data& data) {
    filtered_trucks.reserve(count);
    for (const Truck& truck : data.trucks) {
        if (truck.init_time >= time_bound) {
            continue;
        }
        filtered_trucks.emplace_back(truck);
        if (filtered_trucks.size() == count) {
            break;
        }
    }

    sort(filtered_trucks.begin(), filtered_trucks.end(), [](const Truck& a, const Truck& b) {
        return a.init_time < b.init_time;
    });
}

void filter_orders(size_t count, unsigned int time_bound, std::vector<Order>& filtered_orders, Data& data) {
    filtered_orders.reserve(count);
    for (const Order& order : data.orders) {
        if (order.start_time >= time_bound) {
            continue;
        }
        filtered_orders.emplace_back(order);
        if (filtered_orders.size() == count) {
            break;
        }
    }

    sort(filtered_orders.begin(), filtered_orders.end(), [](const Order& a, const Order& b) {
        return a.start_time < b.start_time;
    });
}


int main() {
    std::string params_path = "./../samples/params.xlsx";
    std::string trucks_path = "./../samples/trucks.xlsx";
    std::string orders_path = "./../samples/orders.xlsx";
    std::string dists_path = "./../samples/distances.xlsx";
    
    Data data(params_path, trucks_path, orders_path, dists_path);

    constexpr unsigned int time_window = 1*24*60;

    std::vector<Truck> trucks;
    filter_trucks(100, 200*time_window, trucks, data);

    std::vector<Order> orders;
    filter_orders(20000, 400*time_window, orders, data);
    
    data.trucks = trucks;
    data.orders = orders;

    std::cout << trucks.size() << std::endl;
    std::cout << orders.size() << std::endl;

    // data.params.DebugPrint();
    // data.trucks.DebugPrint();
    // data.orders.DebugPrint();
    // data.dists.DebugPrint();

    for (Order &order : data.orders) {
        order.obligation = 0;
    }

    BatchSolver solver;
    solution_t solution = solver.Solve(data, time_window);


    Checker checker(data);
    checker.SetSolution(solution);
    auto revenue_raw = checker.Check();
    assert(revenue_raw.has_value());
    
    std::cout << std::fixed << std::setprecision(5) << revenue_raw.value() << std::endl;

    return 0;
}
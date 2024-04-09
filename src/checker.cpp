#include "checker.h"

#ifdef DEBUG_MODE
using std::cout;
using std::endl;
#endif

Checker::Checker(const Data& data) : data_(data) {}

void Checker::SetSolution(const solution_t& solution) {
    solution_ = solution;
}

std::optional<double> Checker::Check() {
    #ifdef DEBUG_MODE
    cout << "##SOLUTION_DEBUG" << endl;
    #endif

    std::set<int> obligation_orders;
    auto& orders_by_truck_pos = solution_.orders_by_truck_pos;
    auto& params = data_.params;
    auto& trucks = data_.trucks;
    auto& orders = data_.orders;
    auto& dists = data_.dists;


    // each truck has its own set of cheduled orders (might be empty set)
    assert(trucks.Size() == orders_by_truck_pos.size());
    size_t trucks_count = trucks.Size();

    int complete_orders_count = 0;
    double summary_revenue = 0;
    for(size_t truck_pos = 0; truck_pos < trucks_count; ++truck_pos) {
        auto &scheduled_orders = orders_by_truck_pos[truck_pos];

        // organizing scheduled orders in the order they will be completed
        sort(scheduled_orders.begin(), scheduled_orders.end(), [&orders](const int& a, const int& b) {
            return orders.GetOrderConst(a).start_time < orders.GetOrderConst(b).start_time;
        });

        complete_orders_count += scheduled_orders.size();

        const Truck& truck = trucks.GetTruckConst(truck_pos);

        #ifdef DEBUG_MODE
        cout << "truck(" << truck.truck_id << "): {";
        for (size_t j = 0; j < scheduled_orders.size(); ++j) {
            cout << orders.GetOrderConst(scheduled_orders[j]).order_id;
            if (j + 1 != scheduled_orders.size()) {
                cout << ", ";
            }
        }
        cout << "}" << endl;

        cout << "initial time(" << truck.init_time << "), city(" << truck.init_city << ")" << endl;
        #endif

        Order previous;
        previous.finish_time = truck.init_time;
        previous.to_city = truck.init_city;
        for (size_t order_pos = 0; order_pos < scheduled_orders.size(); ++order_pos) {
            const Order& current = orders.GetOrderConst(scheduled_orders[order_pos]);

            auto dist_between_orders = dists.GetDistance(previous.to_city, current.from_city); 
            if (!dist_between_orders.has_value()) {
                std::cerr << "checker error truck(" << truck.truck_id << "): no road between " << previous.to_city << " and " << current.from_city << endl;
                exit(1);
            }

            unsigned int arriving_time = previous.finish_time + dist_between_orders.value() * 60 / params.speed;
            if (arriving_time > current.start_time) {
                std::cerr << "checker error truck(" << truck.truck_id << "): arrived too late - time(" << arriving_time << "); order(" << current.order_id << ") starts at " << current.start_time << endl;
                exit(1);
            }

            double revenue = 0.;
            {
                auto cost = data_.MoveBetweenOrders(previous, current);
                assert(cost.has_value());
                revenue += cost.value();
            }
            {
                revenue += data_.GetRealOrderRevenue(scheduled_orders[order_pos]);
            }

            #ifdef DEBUG_MODE
            cout << std::fixed << std::setprecision(5) 
                << "[got " << revenue 
                << "]: arrive at city(" << current.from_city 
                << ") at time(" << arriving_time 
                << ") wait till time(" << current.start_time 
                << ") and move to city(" << current.to_city 
                << ") by time(" << current.finish_time << ")" << endl;
            #endif 

            if (current.obligation) {
                obligation_orders.insert(current.order_id);
            }

            summary_revenue += revenue;
            previous = current;
        }
    }

    int complete_obligation_orders_count = 0;
    for(auto &order : orders) {
        if(order.obligation) {
            ++complete_obligation_orders_count;

            auto it = obligation_orders.find(order.order_id);
            if(it==obligation_orders.end()) {
                std::cerr << "checker error: order(" << order.order_id << ") wasnt scheduled but its obligation one" << std::endl;
            }
        }
    }

    #ifdef DEBUG_MODE
    cout << std::fixed << std::setprecision(5) 
        << "total revenue: " << summary_revenue << endl
        << "complete usual orders(" << complete_orders_count - complete_obligation_orders_count << ") " 
        << "and obligation orders(" << complete_obligation_orders_count << ")" << endl
        << "SOLUTION_DEBUG##" << endl;
    #endif
    return summary_revenue;
}
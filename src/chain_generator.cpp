#include "chain_generator.h"

///////////
// CHAIN //
///////////

#ifdef DEBUG_MODE
using std::cout;
using std::endl;
void Chain::DebugPrint() const {
    std::cout << "  chain: ";
    for (size_t i = 0; i < GetEndPos(); ++i) {
        std::cout << chain[i] << ' ';
    }
    std::cout << '\n';
}
#endif

size_t Chain::EMPTY_EDGE = static_cast<size_t>(-1);

void Chain::Init() {
    chain.fill(Chain::EMPTY_EDGE);
}

Chain::Chain() {
    Init();
}

Chain::Chain(const Chain& other) {
    revenue = other.revenue;
    chain = other.chain;
}

Chain::Chain(std::initializer_list<size_t> l) {
    size_t cur = 0;
    for (size_t order_pos : l) {
        chain[cur++] = order_pos;
    }
    for (; cur < MX_LEN; ++cur) {
        chain[cur] = Chain::EMPTY_EDGE;
    }
}

size_t Chain::GetEndPos() const {
    for(size_t i = 0; i < MX_LEN; ++i) {
        if (chain[i] == Chain::EMPTY_EDGE) {
            return i;
        }
    }
    return MX_LEN;
}


size_t Chain::Back() const {
    return chain[GetEndPos() - 1];
}

void Chain::SetRevenue(double _revenue) {
    revenue = _revenue;
}

/////////////////////
// CHAIN GENERATOR //
/////////////////////

#ifdef DEBUG_MODE
using std::cout;
using std::endl;
void ChainGenerator::DebugPrint() const {
    size_t truck_pos = 0;
    for (const auto& chains : chains_by_truck_pos) {
        std::cout << "truck " << truck_pos << '\n';
        for (const Chain& chain : chains) {
            chain.DebugPrint();
        }
    }
}
#endif

ChainGenerator::ChainGenerator(double min_chain_revenue, const Data& data) : min_chain_revenue_(min_chain_revenue), data_(data) {
    const Trucks& trucks = data_.trucks;

    size_t trucks_count = trucks.Size();
    chains_by_truck_pos.resize(trucks_count);

    InitFirstEdge();

    Merge(MX_LEN - 2);
}

void ChainGenerator::InitFirstEdge() {
    const Trucks& trucks = data_.trucks;
    const Orders& orders = data_.orders;

    const size_t trucks_count = trucks.Size();
    const size_t orders_count = orders.Size();

    for (size_t truck_pos = 0; truck_pos < trucks_count; ++truck_pos) {
        const Truck& truck = trucks.GetTruckConst(truck_pos);

        // our fake first order (state after completing it <=> initial state of truck)
        Order from_order = Solver::make_ffo(truck);

        for (size_t to_order_pos = 0; to_order_pos < orders_count; ++to_order_pos) {
            const Order& to_order = orders.GetOrderConst(to_order_pos);

            auto raw_cost = data_.MoveBetweenOrders(truck, from_order, to_order);
            if (!raw_cost.has_value()) {
                continue;
            }
            double cost = raw_cost.value();

            if (!to_order.obligation && cost - min_chain_revenue_ < 0) {
                continue;
            }

            Chain chain{to_order_pos};
            chain.SetRevenue(cost);

            chains_by_truck_pos[truck_pos].push_back(std::move(chain));
        }
    }
}

void ChainGenerator::Merge(size_t n_times) {
    const Trucks& trucks = data_.trucks;
    const Orders& orders = data_.orders;

    const size_t trucks_count = trucks.Size();
    const size_t orders_count = orders.Size();

    // stores for each order_pos all orders that can go after (also stores revenue addition)
    std::vector<std::vector<std::pair<size_t, double>>> compatible_orders(orders_count);
    for (size_t from_order_pos = 0; from_order_pos < orders_count; ++from_order_pos) {
        const Order& from_order = orders.GetOrderConst(from_order_pos);

        for (size_t to_order_pos = 0; to_order_pos < orders_count; ++to_order_pos) {
            const Order& to_order = orders.GetOrderConst(to_order_pos);

            auto raw_revenue_addition = data_.MoveBetweenOrders(from_order, to_order);
            if (!raw_revenue_addition.has_value()) {
                continue;
            }
            double revenue_addition = raw_revenue_addition.value();
            compatible_orders[from_order_pos].push_back({to_order_pos, revenue_addition});
        }
    }

    // choosing truck
    for (size_t truck_pos = 0; truck_pos < trucks_count; ++truck_pos) {
        const Truck& truck = trucks.GetTruckConst(truck_pos);

        // int i-th merge we suppose to use chains that was produced on (i-1)-th merge
        size_t old_size = 0;
        
        // merging each chain n times
        for (unsigned int i = 0; i < n_times; ++i) {
            size_t cur_size = chains_by_truck_pos[truck_pos].size();

            // choosing chain to merge with
            for (size_t chain_pos = old_size; chain_pos < cur_size; ++chain_pos) {
                // we cant use const reference here because reallocations
                Chain chain = chains_by_truck_pos[truck_pos][chain_pos];
                size_t end_pos = chain.GetEndPos();
                assert(end_pos > 0);
                
                size_t last_order_pos = chain.chain[end_pos - 1];
                const Order& from_order = orders.GetOrderConst(last_order_pos);

                // choosing order to merge chain with
                for (auto [to_order_pos, revenue_bonus] : compatible_orders[last_order_pos]) {
                    const Order& to_order = orders.GetOrderConst(to_order_pos);

                    if (!IsExecutableBy(
                        truck.mask_load_type, 
                        truck.mask_trailer_type, 
                        to_order.mask_load_type, 
                        to_order.mask_trailer_type)
                    ) {  // bad trailer or load type
                        continue;
                    }
                    double revenue = chain.revenue;
                    revenue += revenue_bonus;

                    if (!to_order.obligation && revenue - min_chain_revenue_ < 0) {
                        continue;
                    }

                    Chain new_chain(chain);
                    new_chain.chain[end_pos] = to_order_pos;
                    new_chain.SetRevenue(revenue);

                    chains_by_truck_pos[truck_pos].push_back(std::move(new_chain));
                }
            }

            old_size = cur_size;
        }
    }
}

void ChainGenerator::AddWeightsEdges(Orders& orders, FreeMovementWeightsVectors& edges_w_vecs) {
    assert(ADD_WEIGHTS_EDGES_CALL_COUNT == 0);
    ADD_WEIGHTS_EDGES_CALL_COUNT++;

    auto [additional_orders, free_edge_to_pos] = edges_w_vecs.GetFreeMovementEdges(data_);

    const size_t trucks_count = data_.trucks.Size();
    const size_t main_orders_count = data_.orders.Size();

    for (const Order& order : additional_orders) {
        orders.AddOrder(order);
    }

    for (size_t truck_pos = 0; truck_pos < trucks_count; ++truck_pos) {
        /*
            We want to look at all chains of some truck and add new ones to same set of chains
            we will store count of old chains to iterate only over old ones
        */
        size_t old_chains_count = chains_by_truck_pos[truck_pos].size();
        for (size_t chain_pos = 0; chain_pos < old_chains_count; ++chain_pos) {
            // must make copy here - read logic below
            Chain chain = chains_by_truck_pos[truck_pos][chain_pos];

            size_t end_pos = chain.GetEndPos();
            assert(end_pos > 0);
            size_t last_order_pos = chain.chain[end_pos - 1];

            auto raw_vec = edges_w_vecs.GetWeightsVectorConst(truck_pos, last_order_pos);
            if (!raw_vec.has_value()) {
                continue;
            }
            const weights_vector_t& vec = raw_vec.value();

            /*
                we want to take current chain and produce |S| new chains 
                where S is set of free-movement edges called 'vec' below
                in order to make it faster lets just change current chain for first free-movement edge
                and add (|S| - 1) new chains
            */
            bool first = true;
            for(const auto& [to_city, revenue_bonus] : vec) {
                size_t free_edge_pos = free_edge_to_pos[{truck_pos, last_order_pos, to_city}];
                free_edge_pos += main_orders_count;

                if (first) {
                    chains_by_truck_pos[truck_pos][chain_pos].chain[end_pos] = free_edge_pos;
                    first = false;
                } else {
                    Chain new_chain(chain);
                    new_chain.chain[end_pos] = free_edge_pos;

                    chains_by_truck_pos[truck_pos].push_back(std::move(new_chain));
                }
            }
        }
    }
}
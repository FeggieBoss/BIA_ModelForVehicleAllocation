#include "pre_solver.h"

size_t PreSolver::GetGraphNode(size_t order_pos) const {
    bool is_fo = Solver::IsFakeOrder(order_pos);

    size_t graph_node = order_pos;
    if (is_fo) {
        graph_node = orders_count_ + (order_pos == Solver::flo_pos);
    }
    return graph_node;
}

PreSolver::PreSolver(const std::vector<variable_t>& variables) : variables_(variables) {
    size_t vars_count = variables_.size();
    valid_vars_.resize(vars_count, 0);

    // max trucks/real orders pos
    size_t trucks_mx = 0;
    size_t orders_mx = 0;
    for (size_t var_index = 0; var_index < variables_.size(); ++var_index) {
        const auto& [truck_pos, from_order_pos, to_order_pos] = variables_[var_index];

        trucks_mx = std::max(trucks_mx, truck_pos);

        if (!Solver::IsFakeOrder(from_order_pos)) {
            orders_mx = std::max(orders_mx, from_order_pos);
        }

        if (!Solver::IsFakeOrder(to_order_pos)) {
            orders_mx = std::max(orders_mx, to_order_pos);
        }
    }
    trucks_count_ = trucks_mx + 1;
    orders_count_ = orders_mx + 1;
    

    // +2 because we need to take in account ffo and flo 
    graph_.resize(orders_count_ + 2);
    used_.resize(orders_count_ + 2, false);

    PreSolve();
}

void PreSolver::MarkConnectivityComponent(size_t v) {
    if (used_[v]) {
        return;
    }
    used_[v] = 1;
    for (auto& to_node : graph_[v]) {
        ++valid_vars_[to_node.var_index];
        MarkConnectivityComponent(to_node.to);
    }
}

void PreSolver::ClearGraph() {
    for (auto &arr : graph_) {
        arr.clear();
    }
    std::fill(used_.begin(), used_.end(), false);
}

void PreSolver::PreSolve() {
    for (size_t cur_truck_pos = 0; cur_truck_pos < trucks_count_; ++cur_truck_pos) {
        // marking source component
        {
            for (size_t var_index = 0; var_index < variables_.size(); ++var_index) {
                const auto& [truck_pos, from_order_pos, to_order_pos] = variables_[var_index];
                if (cur_truck_pos != truck_pos) {
                    continue;
                }

                // not taking in account edges leading to sink
                graph_[GetGraphNode(from_order_pos)].push_back(PreSolverNode{
                    GetGraphNode(to_order_pos),  // to
                    var_index
                });
            }

            MarkConnectivityComponent(GetGraphNode(Solver::ffo_pos));

            ClearGraph();
        }
        // marking sink component
        {
            for (size_t var_index = 0; var_index < variables_.size(); ++var_index) {
                const auto& [truck_pos, from_order_pos, to_order_pos] = variables_[var_index];
                if (cur_truck_pos != truck_pos) {
                    continue;
                }

                // not taking in account edges leading to source
                graph_[GetGraphNode(to_order_pos)].push_back(PreSolverNode{
                    GetGraphNode(from_order_pos),  // to
                    var_index
                });
            }

            MarkConnectivityComponent(GetGraphNode(Solver::flo_pos));

            ClearGraph();
        }
    }

    std::vector<variable_t> filtered_variables;
    filtered_variables.reserve(variables_.size());
    for (size_t var_index = 0; var_index < variables_.size(); ++var_index) {
        if (valid_vars_[var_index] == 2) {
            filtered_variables.emplace_back(std::move(variables_[var_index]));
        }
    }
    variables_ = filtered_variables;
}

std::vector<variable_t> PreSolver::GetFilteredVariables() const {
    return variables_;
}
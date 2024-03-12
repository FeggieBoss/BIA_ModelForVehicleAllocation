#ifndef DEFINE_WEIGHTED_CITIES_SOLVER_H
#define DEFINE_WEIGHTED_CITIES_SOLVER_H

#include "main.h"
#include "solver.h"

#include <set>

typedef std::vector<double> weights_vector_t;


class WeightedCitiesSolver : public Solver {
private:
    // if truck choose to move without order to another city we set limit of time he has to do so
    std::optional<unsigned int> free_move_time_boundary_ = std::nullopt;
    // weights of last cities in trucks schedules
    weights_vector_t w_;
    Data data_;
    // index of variable in vector X -> its 3d/variable_t representation
    std::unordered_map<size_t, variable_t> to_3d_variables_;

    HighsModel CreateModel();
public:
    WeightedCitiesSolver();
    
    void SetData(const Data& data) override;
    void SetData(const Data& data, unsigned int time_boundary);
    void SetVectorW(const weights_vector_t& w);
    solution_t Solve() override;
};

#endif // DEFINE_WEIGHTED_CITIES_SOLVER_H
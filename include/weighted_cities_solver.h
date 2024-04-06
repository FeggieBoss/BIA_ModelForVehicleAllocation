#ifndef DEFINE_WEIGHTED_CITIES_SOLVER_H
#define DEFINE_WEIGHTED_CITIES_SOLVER_H

#include "main.h"
#include "solver.h"

#include <set>

typedef std::vector<double> weights_vector_t;


class WeightedCitiesSolver : public Solver {
private:
    // if truck choose to move without order to another city we set limit of time he has to do so
    std::optional<unsigned int> time_boundary_ = std::nullopt;
    // weights of last cities in trucks schedules
    weights_vector_t w_;
    Data data_;
    // index of variable in vector X -> its 3d/variable_t representation
    std::unordered_map<size_t, variable_t> to_3d_variables_;

    HighsModel CreateModel();
public:
    WeightedCitiesSolver();
    
    void ModifyData(Data& data);

    /*
        No time boundary means no fine for waiting after completing last order
        With VectorW = 0 will produce same solution as HonestSolver
    */
    void SetData(const Data& data) override;

    /*
        Adding fine for waiting after completing last order until time_boundary
        Note (1) that it also prohibit trucks to freely move between cities in case finish_time > time_boundary
             (2) its expected that for all orders: order.start_time <= time_boundary
    */
    void SetData(const Data& data, unsigned int time_boundary);

    void SetVectorW(const weights_vector_t& w);
    Data GetData();

    solution_t Solve() override;
};

#endif // DEFINE_WEIGHTED_CITIES_SOLVER_H
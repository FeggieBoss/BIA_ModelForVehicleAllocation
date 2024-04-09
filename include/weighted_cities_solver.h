#ifndef DEFINE_WEIGHTED_CITIES_SOLVER_H
#define DEFINE_WEIGHTED_CITIES_SOLVER_H

#include "main.h"
#include "honest_solver.h"

#include <set>

/*
    Provides weights for all cities parameterized by {truck_id, order_id}
    It says how much weight city will have for truck with 'truck_id'
    if this truck will go in this city right after making its final order with 'order_id' 
*/
typedef std::map<std::pair<unsigned int, unsigned int>, double> weights_vector_t;
class CitiesWeightsVectors {
private:
    std::vector<weights_vector_t> cities_w_vecs_;
public:
    CitiesWeightsVectors();
    CitiesWeightsVectors(size_t cities_count);
    CitiesWeightsVectors(const CitiesWeightsVectors& other);
    const CitiesWeightsVectors& operator=(const CitiesWeightsVectors& other);
    
    bool IsInitialized() const;
    std::optional<double> GetWeight(unsigned int city_id, unsigned int truck_id, unsigned int order_id) const;
    const weights_vector_t& GetWeightsVector(unsigned int city_id) const;
    void AddWeight(unsigned int city_id, unsigned int truck_id, unsigned int order_id, double weight);
    void Reset();
};

/*
    !!!Note!!!: parents method "solution_t Solve()" will produce unexpected solution
    Solution will be generated according to modified Data (Data after ModifyData method)
    But ModifyData provides such constraints which helps to work with such solution:
    (1) Free-movement orders always being added right after all real orders
    (2) Free-movement orders always have order_id = 0
    There is two ways of working with such solution:
    1. We dont want to use free-movement orders:
        we should just use our unmodified Data
        and when we encounter orders | order_pos >= data.orders.Size() we should just ignore such orders
        (we are using (1) for correctness here)
    2. We want to use free-movement orders:
        we should get Data with free-movement orders in it (Data after 'ModifyData') by GetDataConst method
        and then simply using it as our Data
        (we can distinguish free-movement order because all of them have 'order_id = 0' as (2) claims)
*/
class WeightedCitiesSolver : public Solver {
private:
    // if truck choose to move without order to another city we set limit of time he has to do so
    std::optional<unsigned int> time_boundary_ = std::nullopt;
    // weights of last cities in trucks schedules
    CitiesWeightsVectors cities_ws_;

    void ModifyData(Data& data) const;
public:
    WeightedCitiesSolver();
    
    const Data& GetDataConst() const;

    /*
        No time boundary means no fine for waiting after completing last order
        With CitiesWeightsVectors = 0 will produce same solution as HonestSolver
        !!!Note!!!:
        (1) ModifyData being called
        (2) CitiesWeightsVectors will be all zeros (if u want non zero one call other method)
    */
    void SetData(const Data& data) override;

    /*
        Adding fine for waiting after completing last order until time_boundary
        Note (1) that it also prohibit trucks to freely move between cities in case finish_time > time_boundary
          !!!(2)!!! ModifyData being called
    */
    void SetData(const Data& data, unsigned int time_boundary, const CitiesWeightsVectors& cities_ws);
    
    HighsModel CreateModel() override;
};

#endif // DEFINE_WEIGHTED_CITIES_SOLVER_H
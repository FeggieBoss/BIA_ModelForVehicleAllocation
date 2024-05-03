#ifndef DEFINE_SOLVER_H
#define DEFINE_SOLVER_H

#include "Highs.h"
#include "main.h"
#include "solution.h"
#include "data.h"

#include <unordered_set>

namespace std {
    namespace
    {
        // Code from boost
        // Reciprocal of the golden ratio helps spread entropy
        //     and handles duplicates.
        // See Mike Seymour in magic-numbers-in-boosthash-combine:
        //     https://stackoverflow.com/questions/4948780

        template <class T>
        inline void hash_combine(std::size_t& seed, T const& v)
        {
            seed ^= hash<T>()(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
        }

        // Recursive template code derived from Matthieu M.
        template <class Tuple, size_t Index = std::tuple_size<Tuple>::value - 1>
        struct HashValueImpl
        {
          static void apply(size_t& seed, Tuple const& tuple)
          {
            HashValueImpl<Tuple, Index-1>::apply(seed, tuple);
            hash_combine(seed, get<Index>(tuple));
          }
        };

        template <class Tuple>
        struct HashValueImpl<Tuple,0>
        {
          static void apply(size_t& seed, Tuple const& tuple)
          {
            hash_combine(seed, get<0>(tuple));
          }
        };
    }

    template <typename ... TT>
    struct hash<std::tuple<TT...>> 
    {
        size_t
        operator()(std::tuple<TT...> const& tt) const
        {                                              
            size_t seed = 0;                             
            HashValueImpl<std::tuple<TT...> >::apply(seed, tt);    
            return seed;                                 
        }                                              

    };
}

// weights of cities
typedef std::map<unsigned int, double> weights_vector_t;
/*
    Provides weights for all cities parameterized by {truck_pos, order_pos}
    It says how much weight city will have for truck with 'truck_pos'
    if this truck will go in this city right after making its final order with 'order_pos' 
*/
class FreeMovementWeightsVectors {
private:
    std::map<std::pair<size_t, size_t>, weights_vector_t> edges_w_vecs_;
public:
    FreeMovementWeightsVectors();
    FreeMovementWeightsVectors(const FreeMovementWeightsVectors& other);
    const FreeMovementWeightsVectors& operator=(const FreeMovementWeightsVectors& other);

    std::map<std::pair<size_t, size_t>, weights_vector_t>::const_iterator begin() const {  // NOLINT
        return edges_w_vecs_.begin();
    }

    std::map<std::pair<size_t, size_t>, weights_vector_t>::const_iterator end() const {  // NOLINT
        return edges_w_vecs_.end();
    }

    /*
        Generate free-movement orders based on weights vectors
        Also provides mapping from free-movement edge to its position in orders set:
        {truck_pos, order_pos, city_id} -> free_movement_order_pos
    */
    std::pair<Orders, std::unordered_map<std::tuple<size_t, size_t, unsigned int>, size_t>> GetFreeMovementEdges(const Data& data) const;
    
    bool IsInitialized() const;
    std::optional<double> GetWeight(size_t truck_pos, size_t order_pos, unsigned int city_id) const;
    std::optional<std::reference_wrapper<weights_vector_t>> GetWeightsVector(size_t truck_pos, size_t order_pos);
    std::optional<std::reference_wrapper<const weights_vector_t>> GetWeightsVectorConst(size_t truck_pos, size_t order_pos) const;
    void AddWeight(size_t truck_pos, size_t order_pos, unsigned int city_id, double weight);
    void Reset();
};

// {truck_pos, from_order_pos, to_order_pos}
typedef std::tuple<size_t, size_t, size_t> variable_t;

class Solver {
public:
    static size_t ffo_pos;
    static size_t flo_pos;
    static std::function<Order(const Truck&)> make_ffo;
    static std::function<Order(const Order&)> make_flo;

    static bool IsFakeOrder(size_t order_pos);

    Data data_;
    // index of variable in vector X -> its 3d/variable_t representation
    std::unordered_map<size_t, variable_t> to_3d_variables_;

public:
    Solver() = default;
    virtual void SetData(const Data& data) = 0;
    virtual HighsModel CreateModel() = 0;

    solution_t Solve();
};

#endif // DEFINE_SOLVER_H

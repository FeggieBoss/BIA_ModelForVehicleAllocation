#ifndef DEFINE_SOLVER_H
#define DEFINE_SOLVER_H

#include "Highs.h"
#include "main.h"
#include "solution.h"
#include "data.h"

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

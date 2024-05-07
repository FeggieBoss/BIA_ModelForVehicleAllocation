#ifndef DEFINE_CHAIN_GENERATOR_H
#define DEFINE_CHAIN_GENERATOR_H

#include "solver.h"

#include <array>

#ifdef TEST_BUILD
constexpr unsigned int MX_LEN = 6;
#else
constexpr unsigned int MX_LEN = 3;
#endif

class Chain {
public:
    static size_t EMPTY_EDGE;
    std::array<size_t, MX_LEN> chain;
    double revenue = 0.f;
    
    Chain();
    Chain(const Chain& other);
    Chain(std::initializer_list<size_t> l);

    void SetRevenue(double revenue);
    size_t GetEndPos() const;
    size_t Back() const;
    size_t operator[](size_t pos) const;
    size_t& operator[](size_t pos);

    #ifdef DEBUG_MODE
    void DebugPrint() const;
    #endif
private:
    void Init();
};

class ChainGenerator {
public:
    std::vector<std::vector<Chain>> chains_by_truck_pos;

    ChainGenerator(double min_chain_revenue, size_t mx_chain_len);

    void GenerateChains(const Data& data);
    /*
        will generate new orders (free movement edges) and new chains with them so we need non constant reference here
        Note: 
        (1) cant be called twice for same Data
        (2) there is three possible situation for old chain
            (2.1) stays untouched at same position (there is no free-movement edges for such chain)
            (2.2) can grow but still remains its position (exactly one free-movement edge for such chain)
            (2.3) more than one new chain will be produced based on this chain (multiple free-movement edges)
    */
    void AddWeightsEdges(Data& data, const FreeMovementWeightsVectors& edges_w_vecs);

    #ifdef DEBUG_MODE
    void DebugPrint() const;
    #endif
private:
    int ADD_WEIGHTS_EDGES_CALL_COUNT = 0;
    double min_chain_revenue_;
    size_t mx_chain_len_;

    void InitFirstEdge(const Data& data);
    void Merge(const Data& data, size_t n_times);
};

#endif // DEFINE_CHAIN_GENERATOR_H

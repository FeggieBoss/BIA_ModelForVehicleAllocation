#include <gtest/gtest.h>

#include "checker.h"
#include "batch_solver.h"
#include "chain_solver.h"

class SmallDataTest : public testing::Test {
private:
    void SetUpParams(Params& params) {
        params = Params{
            60.,  // speed
            1.,   // free_km_cost
            0.,   // free_hour_cost
            60.,  // wait_cost
            0.,   // duty_km_cost
            0.    // duty_hour_cost
        };
    }

    void SetUpTrucks(Trucks& trucks) {
        trucks = Trucks({
            //   (truck_id, load_type, trailer_type  , init_time, init_city)
            Truck(1       , "Полная" , "Рефрижератор", 0        , 1          ),
            Truck(2       , "Полная" , "Рефрижератор", 90       , 2          )
        });
    }

    void SetUpOrders(Orders& orders) {
        orders = Orders({
            //   (order_id, obligation, start_time, finish_time, from_city, to_city, load_type, trailer_type  , distance, revenue)
            Order(1       , false     , 35        , 50         , 3        , 4      , "Полная" , "Рефрижератор", 10.     , 40.    ),
            Order(2       , false     , 55        , 70         , 4        , 5      , "Полная" , "Рефрижератор", 10.     , 10.    ),
            Order(3       , false     , 120       , 300        , 6        , 7      , "Полная" , "Рефрижератор", 30.     , 60.    ),
            Order(4       , true      , 120       , 130        , 5        , 6      , "Полная" , "Рефрижератор", 10.     , 20.    )
        });
    }

    void SetUpDistances(Distances& distances) {
        distances.dists = {
         // {{from_id, to_id}, distance}
            {{1      , 2    }, 10.     },
            {{1      , 3    }, 30.     },
            {{1      , 4    }, 30.     },
            {{1      , 5    }, 30.     },
            {{1      , 6    }, 30.     },
            {{1      , 7    }, 60.     },
            {{2      , 1    }, 10.     },
            {{2      , 3    }, 45.     },
            {{2      , 4    }, 30.     },
            {{2      , 5    }, 30. + 1.}, //  Note: this change was made to make unique solution with best revenue 
            {{2      , 6    }, 30.     },
            {{2      , 7    }, 60.     },
            {{3      , 1    }, 30.     },
            {{3      , 2    }, 45.     },
            {{3      , 4    }, 10.     },
            {{3      , 5    }, 20.     },
            {{3      , 6    }, 30.     },
            {{3      , 7    }, 30.     },
            {{4      , 1    }, 30.     },
            {{4      , 2    }, 30.     },
            {{4      , 3    }, 10.     },
            {{4      , 5    }, 10.     },
            {{4      , 6    }, 20.     },
            {{4      , 7    }, 30.     },
            {{5      , 1    }, 30.     },
            {{5      , 2    }, 30.     },
            {{5      , 3    }, 20.     },
            {{5      , 4    }, 10.     },
            {{5      , 6    }, 10.     },
            {{5      , 7    }, 30.     },
            {{6      , 1    }, 30.     },
            {{6      , 2    }, 30.     },
            {{6      , 3    }, 30.     },
            {{6      , 4    }, 20.     },
            {{6      , 5    }, 10.     },
            {{6      , 7    }, 30.     },
            {{7      , 1    }, 60.     },
            {{7      , 2    }, 60.     },
            {{7      , 3    }, 30.     },
            {{7      , 4    }, 30.     },
            {{7      , 5    }, 30.     },
            {{7      , 6    }, 30.     }
        };
    }

    void SetUpSolution(solution_t& solution) {
        solution = {{
            {0, 1, 3}, // first truck orders positions
            {2}        // second truck orders positions
        }};
    }

public:
    Data data_;
    solution_t expected_;

    void SetUp() override {
        SetUpParams(data_.params);
        SetUpTrucks(data_.trucks);
        SetUpOrders(data_.orders);
        SetUpDistances(data_.dists);
        data_.cities_count = 7;


        SetUpSolution(expected_);
    }

    // void TearDown() override {}
};

TEST_F(SmallDataTest, FlowSolverTestBasic) {
    FlowSolver solver;
    solver.SetData(data_);
    auto model = solver.CreateModel();

    // data_.params.DebugPrint();
    // data_.trucks.DebugPrint();
    // data_.orders.DebugPrint();
    // data_.dists.DebugPrint();

    solution_t solution = solver.Solve(model);

    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for SmallData";
}

TEST_F(SmallDataTest, WeightedSolverTestBasic) {
    WeightedCitiesSolver solver;
    solver.SetData(data_);
    solution_t solution = solver.Solve();

    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for SmallData";
}

// edges_w_vecs = 0, time_boundary = 0 => nothing change
TEST_F(SmallDataTest, WeightedSolverTestTimeBoundary1) {
    FreeMovementWeightsVectors edges_w_vecs;
    unsigned int time_boundary = 0;

    WeightedCitiesSolver solver;
    solver.SetData(data_, time_boundary, edges_w_vecs);
    solution_t solution = solver.Solve();

    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for SmallData (edges_w_vecs = 0, time_boundary = 0 => suppose to change nothing)";
}

// edges_w_vecs = 0, time_boundary is big => nothing change
TEST_F(SmallDataTest, WeightedSolverTestTimeBoundary2) {
    FreeMovementWeightsVectors edges_w_vecs;
    unsigned int time_boundary = 1000;

    WeightedCitiesSolver solver;
    solver.SetData(data_, time_boundary, edges_w_vecs);
    solution_t solution = solver.Solve();

    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for SmallData (edges_w_vecs = 0, time_boundary is big => suppose to change nothing)";
}

// edges_w_vecs non zero, time_boundary is big
TEST_F(SmallDataTest, WeightedSolverTestCitiesWs) {
    const Trucks& trucks = data_.trucks;
    const Orders& orders = data_.orders;
    const size_t cities_count = data_.cities_count;
    
    const auto& expected0 = expected_.orders_by_truck_pos[0];
    const auto& expected1 = expected_.orders_by_truck_pos[1];

    FreeMovementWeightsVectors edges_w_vecs;
    unsigned int time_boundary = 1000;
    
    /*
        we want to add weight for first truck and his last order in ideal solution (lets call it just "last") for each city
        <=> edges_w_vecs.AddWeight(truck_pos=1, order_pos=last_pos, city_id=..., weight=...)
        <=> forcing truck to move in this city after completing last
        (if we are already in this city WeightedSolver will generate loop with positive weight - still must take it)
    */
    size_t last_pos = expected0.back();
    size_t truck1_pos = 0;
    unsigned int last_city = orders.GetOrderConst(last_pos).to_city;
    for (unsigned int city = 1; city <= cities_count; ++city) {
        // any non zero weight suppose to force truck going there
        edges_w_vecs.AddWeight(truck1_pos, last_pos, city, 1.); 

        // setting up solver
        WeightedCitiesSolver solver;
        solver.SetData(data_, time_boundary, edges_w_vecs);
        solution_t solution = solver.Solve();
        
        const auto& solution0 = solution.orders_by_truck_pos[0];
        const auto& solution1 = solution.orders_by_truck_pos[1];

        // checking valid prefix of real orders
        ASSERT_EQ(expected0.size() + 1, solution0.size());
        for (size_t i = 0; i < expected0.size(); ++i) {
            EXPECT_EQ(expected0[i], solution0[i]);
        }

        // checking free-movement order in the back to our city
        const Data& modified_data = solver.GetDataConst();
        const Order& free_movement = modified_data.orders.GetOrderConst(solution0.back());
        EXPECT_EQ(city, free_movement.to_city);

        // other truck still suppose to complete 3rd order and just wait till time_boundary
        EXPECT_EQ(expected1, solution1) << "Suppose to be ideal solution for SmallData and truck_id = 2 (we dont add any weight vectors for truck_id = 2 => suppose to change nothing)";

        edges_w_vecs.Reset();
    }
}

TEST_F(SmallDataTest, CheckerTest) {
    Checker checker(data_);
    checker.SetSolution(expected_);

    auto revenue_raw = checker.Check();
    ASSERT_TRUE(revenue_raw.has_value());
    EXPECT_DOUBLE_EQ(10., revenue_raw.value());
}

TEST_F(SmallDataTest, ChainGeneratorNoFreeMovementEdgesTest) {
    ChainGenerator chain_generator(0.f, 4);
    chain_generator.GenerateChains(data_);

    const auto& chains0 = chain_generator.chains_by_truck_pos[0];

    std::vector<std::vector<size_t>> expected0{
        {0},
        {3},
        {0, 1},
        {0, 3},
        {0, 1, 2},
        {0, 1, 3}
    };
    ASSERT_EQ(expected0.size(), chains0.size());
    for (size_t i = 0; i < expected0.size(); ++i) {
        for (size_t j = 0; j < expected0[i].size(); ++j) {
            EXPECT_EQ(expected0[i][j], chains0[i].chain[j]);
        }
    }
    
    const auto& chains1 = chain_generator.chains_by_truck_pos[1];
    ASSERT_EQ(1, chains1.size());
    // {2}
    EXPECT_EQ(2, chains1[0].Back());
}

TEST_F(SmallDataTest, ChainGeneratorSingleFreeMovementEdgeTest) {
    ChainGenerator old_chain_generator(0.f, 4);
    old_chain_generator.GenerateChains(data_);

    size_t old_orders_count = data_.orders.Size();

    for (size_t truck_pos = 0; truck_pos < data_.trucks.Size(); ++truck_pos) {
        const std::vector<Chain>& old_chains = old_chain_generator.chains_by_truck_pos[truck_pos];

        for (size_t chain_pos = 0; chain_pos < old_chains.size(); ++chain_pos) {
            const Chain& old_chain = old_chains[chain_pos];
            size_t old_chain_len = old_chain.GetEndPos();
            size_t last_order_pos = old_chain.chain[old_chain_len - 1];
            
            // adding edge that suppose to grow current chain
            FreeMovementWeightsVectors edges_w_vecs;
            unsigned city_id = 1;
            edges_w_vecs.AddWeight(truck_pos, last_order_pos, city_id, 1.);

            ChainGenerator chain_generator(0.f, 4);
            Data data(data_);
            chain_generator.GenerateChains(data);
            chain_generator.AddWeightsEdges(data, edges_w_vecs);
            // checking that it produced exactly one free-movement edge
            ASSERT_EQ(old_orders_count + 1, data.orders.Size()) << "Only one free-movement edge supposed to be produced";

            const Order& last_order = data_.orders.GetOrderConst(last_order_pos);
            const Order& free_movement_order = data.orders.GetOrderConst(old_orders_count);
            EXPECT_EQ(last_order.to_city, free_movement_order.from_city);
            EXPECT_EQ(last_order.finish_time, free_movement_order.start_time);
            EXPECT_EQ(city_id, free_movement_order.to_city);
            
            const std::vector<Chain>& new_chains = chain_generator.chains_by_truck_pos[truck_pos];
            ASSERT_EQ(old_chains.size(), new_chains.size()) << "The number of chains must have remained same";
            /*
                !!!NOTE!!!
                we are using here that chain generator saving relative order of chains 
            */
            const Chain& new_chain = new_chains[chain_pos];
            // checking that chain got bigger
            ASSERT_EQ(old_chain_len + 1, new_chain.GetEndPos()) << "Chain must have lengthened";
            // checking its last order is exactly new free-movement edge
            EXPECT_EQ(data_.orders.Size(), new_chain.Back()) << "Last order must be exactly new free-movement edge";
        }
    }
}

TEST_F(SmallDataTest, ChainGeneratorTest) {
    ChainGenerator chain_generator(0.f, 4);
    chain_generator.GenerateChains(data_);

    const size_t truck_pos = 1;
    const size_t order_pos = 2; 
    const auto& chains1 = chain_generator.chains_by_truck_pos[truck_pos];
    ASSERT_EQ(1, chains1.size());
    EXPECT_EQ(order_pos, chains1[0].Back());


    FreeMovementWeightsVectors edges_w_vecs;
    for (unsigned int city_id = 1; city_id <= data_.cities_count; ++city_id) {
        edges_w_vecs.AddWeight(truck_pos, order_pos, city_id, 1.);
    }

    chain_generator.AddWeightsEdges(data_, edges_w_vecs);

    std::set<unsigned int> cities;
    for (const Chain& chain : chain_generator.chains_by_truck_pos[truck_pos]) {
        cities.emplace(data_.orders.GetOrderConst(chain.Back()).to_city);
    }

    unsigned int cur = 1;
    for (unsigned int city_id : cities) {
        EXPECT_EQ(city_id, cur++) << "All cities have to be presented";
    }
}

TEST_F(SmallDataTest, ChainSolverNoFreeMovementEdgesTest) {
    ChainSolver solver(-1e9, 4);
    solver.SetData(data_);

    solution_t solution = solver.Solve();
    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for SmallData";
}

TEST_F(SmallDataTest, ChainSolverTest) {
    /*
        solution without this free-movement edge {0,1,3} and {2} for truck_pos 0 and 1 respectively
        lets add free-movement edges for truck_pos = 1 and check that solver used it
    */
    size_t truck_pos = 1;
    for (size_t order_pos = 0; order_pos < data_.orders.Size(); ++order_pos) {
        unsigned int from_city = data_.orders.GetOrderConst(order_pos).to_city;

        for (unsigned int city_id = 0; city_id < data_.cities_count; ++city_id) {
            // we have to provide free-movement edges between cities with road between them
            if (!data_.dists.GetDistance(from_city, city_id).has_value()) {
                continue;
            }

            ChainSolver solver(-1e9, 4);

            FreeMovementWeightsVectors edges_w_vecs;
            edges_w_vecs.AddWeight(truck_pos, order_pos, city_id, 1.);

            solver.SetData(data_, edges_w_vecs);

            solution_t solution = solver.Solve();

            const auto& solution0 = solution.orders_by_truck_pos[0];
            const auto& solution1 = solution.orders_by_truck_pos[1];

            const auto& expected0 = expected_.orders_by_truck_pos[0];
            const auto& expected1 = expected_.orders_by_truck_pos[1];

            EXPECT_EQ(expected0, solution0) << "Suppose to be ideal solution for SmallData for truck_pos = 0";
            if (order_pos == expected1.back()) {
                ASSERT_EQ(expected1.size() + 1, solution1.size()) << "truck_pos = 1 suppose to use free-movement edge";
                EXPECT_EQ(data_.orders.Size(), solution1.back());
            } else {
                EXPECT_EQ(expected1, solution1) << "Suppose to be ideal solution for SmallData for truck_pos = 1";
            }
        }
    }
}

TEST_F(SmallDataTest, BatchSolverFlowTestOneBatch) {
    std::shared_ptr<WeightedCitiesSolver> solver = std::make_shared<WeightedCitiesSolver>();
    BatchSolver batch_solver(std::move(solver));

    // big time bound
    solution_t solution = batch_solver.Solve(data_, 1000);
    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for SmallData";

    // time bound = max(for order in orders {order.finish_time})
    solution = batch_solver.Solve(data_, 300);
    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for SmallData";

    // time bound = max(for order in orders {order.start_time}) + 1
    solution = batch_solver.Solve(data_, 121);
    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for SmallData (time bound = max(for order in orders {order.start_time}) + 1 still suppose to produce only one batch)";
}

TEST_F(SmallDataTest, BatchSolverAssignmentTestOneBatch) {
    std::shared_ptr<ChainSolver> solver = std::make_shared<ChainSolver>(-1e9, 4);
    BatchSolver batch_solver(std::move(solver));

    // big time bound
    solution_t solution = batch_solver.Solve(data_, 1000);
    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for SmallData";

    // time bound = max(for order in orders {order.finish_time})
    solution = batch_solver.Solve(data_, 300);
    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for SmallData";

    // time bound = max(for order in orders {order.start_time}) + 1
    solution = batch_solver.Solve(data_, 121);
    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for SmallData (time bound = max(for order in orders {order.start_time}) + 1 still suppose to produce only one batch)";
}

TEST_F(SmallDataTest, BatchSolverFlowTest) {
    std::shared_ptr<WeightedCitiesSolver> solver = std::make_shared<WeightedCitiesSolver>();
    BatchSolver batch_solver(std::move(solver));

    for (unsigned int time_bound = 5; time_bound <= 300; time_bound += 5) {
        solution_t solution = batch_solver.Solve(data_, time_bound);
        EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for SmallData and time_bound = " << time_bound;
    }
}

TEST_F(SmallDataTest, BatchSolverAssignmentTest) {
    std::shared_ptr<ChainSolver> solver = std::make_shared<ChainSolver>(-1e9, 4);
    BatchSolver batch_solver(std::move(solver));

    for (unsigned int time_bound = 5; time_bound <= 300; time_bound += 5) {
        solution_t solution = batch_solver.Solve(data_, time_bound);
        EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for SmallData and time_bound = " << time_bound;
    }
}
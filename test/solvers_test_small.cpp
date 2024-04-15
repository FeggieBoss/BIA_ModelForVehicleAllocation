#include <gtest/gtest.h>

#include "checker.h"
#include "batch_solver.h"

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

TEST_F(SmallDataTest, HonestSolverTestBasic) {
    HonestSolver solver;
    solver.SetData(data_);

    // data_.params.DebugPrint();
    // data_.trucks.DebugPrint();
    // data_.orders.DebugPrint();
    // data_.dists.DebugPrint();

    solution_t solution = solver.Solve();

    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for SmallData)";
}

TEST_F(SmallDataTest, WeightedSolverTestBasic) {
    WeightedCitiesSolver solver;
    solver.SetData(data_);
    solution_t solution = solver.Solve();

    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for SmallData";
}

// cities_ws = 0, time_boundary = 0 => nothing change
TEST_F(SmallDataTest, WeightedSolverTestTimeBoundary1) {
    FreeMovementWeightsVectors cities_ws;
    unsigned int time_boundary = 0;

    WeightedCitiesSolver solver;
    solver.SetData(data_, time_boundary, cities_ws);
    solution_t solution = solver.Solve();

    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for SmallData (cities_ws = 0, time_boundary = 0 => suppose to change nothing)";
}

// cities_ws = 0, time_boundary is big => nothing change
TEST_F(SmallDataTest, WeightedSolverTestTimeBoundary2) {
    FreeMovementWeightsVectors cities_ws;
    unsigned int time_boundary = 1000;

    WeightedCitiesSolver solver;
    solver.SetData(data_, time_boundary, cities_ws);
    solution_t solution = solver.Solve();

    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for SmallData (cities_ws = 0, time_boundary is big => suppose to change nothing)";
}

// cities_ws non zero, time_boundary is big
TEST_F(SmallDataTest, WeightedSolverTestCitiesWs) {
    const Trucks& trucks = data_.trucks;
    const Orders& orders = data_.orders;
    const size_t cities_count = data_.cities_count;
    
    const auto& expected0 = expected_.orders_by_truck_pos[0];
    const auto& expected1 = expected_.orders_by_truck_pos[1];

    FreeMovementWeightsVectors cities_ws(cities_count);
    unsigned int time_boundary = 1000;
    
    /*
        we want to add weight for first truck and his last order in ideal solution (lets call it just "last") for each city
        <=> cities_ws.AddWeight(city_id=..., truck_pos=1, order_pos=last_pos, weight=...)
        <=> forcing truck to move in this city after completing last (why? - look below)
        
        params.wait_cost = 60 => 1min = 1$
        params.free_km_cost = 1, params.free_hour_cost = 0, params.speed = 60 => 1km = 1$ and we spend 1min moving 1km => 1min = 1$ of free-ride
        So its same amount of money either to just wait T minutes, or spend T minutes freely riding between cities
        => if we have additional moneys for finishing in city and have unlimited time_boundary to move there its always better to move there
    */
    size_t last_pos = expected0.back();
    size_t truck1_pos = 0;
    unsigned int last_city = orders.GetOrderConst(last_pos).to_city;
    for (unsigned int city = 1; city <= cities_count; ++city) {
        // any non zero weight suppose to force truck going there
        cities_ws.AddWeight(city, truck1_pos, last_pos, 1.); 

        // setting up solver
        WeightedCitiesSolver solver;
        solver.SetData(data_, time_boundary, cities_ws);
        solution_t solution = solver.Solve();
        
        const auto& solution0 = solution.orders_by_truck_pos[0];
        const auto& solution1 = solution.orders_by_truck_pos[1];

        // checking if we are already in city we want to force truck going
        if (city == last_city) {
            EXPECT_EQ(expected0, solution0) << "Suppose to be ideal solution for SmallData and truck_id = 1 (we add weight in same city this truck will end his scheduling plan => suppose to change nothing)";
        } else {
            // checking valid prefix of real orders
            ASSERT_EQ(expected0.size() + 1, solution0.size());
            for (size_t i = 0; i < expected0.size(); ++i) {
                EXPECT_EQ(expected0[i], solution0[i]);
            }

            // checking free-movement order in the back to our city
            const Data& modified_data = solver.GetDataConst();
            const Order& free_movement = modified_data.orders.GetOrderConst(solution0.back());
            EXPECT_EQ(city, free_movement.to_city);
        }

        // other truck still suppose to complete 3rd order and just wait till time_boundary
        EXPECT_EQ(expected1, solution1) << "Suppose to be ideal solution for SmallData and truck_id = 2 (we dont add any weight vectors for truck_id = 2 => suppose to change nothing)";

        cities_ws.Reset();
    }
}

TEST_F(SmallDataTest, BatchSolverTestOneBatch) {
    BatchSolver solver;

    // big time bound
    solution_t solution = solver.Solve(data_, 1000);
    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for SmallData";

    // time bound = max(for order in orders {order.finish_time})
    solution = solver.Solve(data_, 300);
    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for SmallData";

    // time bound = max(for order in orders {order.start_time}) + 1
    solution = solver.Solve(data_, 121);
    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for SmallData (time bound = max(for order in orders {order.start_time}) + 1 still suppose to produce only one batch)";
}

TEST_F(SmallDataTest, BatchSolverTest) {
    BatchSolver solver;

    for (unsigned int time_bound = 5; time_bound <= 300; time_bound += 5) {
        solution_t solution = solver.Solve(data_, time_bound);
        EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for SmallData and time_bound = " << time_bound;
    }
}

TEST_F(SmallDataTest, CheckerTest) {
    Checker checker(data_);
    checker.SetSolution(expected_);

    auto revenue_raw = checker.Check();
    ASSERT_TRUE(revenue_raw.has_value());
    EXPECT_DOUBLE_EQ(10., revenue_raw.value());
}
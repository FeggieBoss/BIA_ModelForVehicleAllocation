#include <gtest/gtest.h>

#include "checker.h"
#include "batch_solver.h"

class TrickyDataTest : public testing::Test {
private:
    void SetUpParams(Params& params) {
        params = Params{
            60.,  // speed
            1.,   // free_km_cost
            120., // free_hour_cost
            60.,  // wait_cost
            0.1,  // duty_km_cost
            60.   // duty_hour_cost
        };
    }

    void SetUpTrucks(Trucks& trucks) {
        trucks = Trucks({
            //   (truck_id, load_type, trailer_type  , init_time, init_city)
            Truck(1       , "Полная" , "Рефрижератор", 0        , 4          ),
            Truck(2       , "Полная" , "Рефрижератор", 0        , 1          )
        });
    }

    void SetUpOrders(Orders& orders) {
        orders = Orders({
            //   (order_id, obligation, start_time, finish_time, from_city, to_city, load_type, trailer_type  , distance, revenue)
            Order(1       , false     , 40        , 80         , 2        , 3      , "Полная" , "Рефрижератор", 0.      , 20.    ),
            Order(2       , false     , 90        , 100        , 4        , 3      , "Полная" , "Рефрижератор", 10.     , 0.     ),
            Order(3       , false     , 100       , 115        , 3        , 2      , "Полная" , "Рефрижератор", 10.     , 0.     ),
            Order(4       , false     , 115       , 141        , 2        , 1      , "Полная" , "Рефрижератор", 10.     , 5.     ),
            Order(5       , false     , 140       , 150        , 1        , 2      , "Полная" , "Рефрижератор", 10.     , 0.     ),
            Order(6       , false     , 150       , 160        , 2        , 3      , "Полная" , "Рефрижератор", 10.     , 0.     ),
            Order(7       , false     , 160       , 170        , 3        , 4      , "Полная" , "Рефрижератор", 10.     , 64.    ),
            Order(8       , false     , 200       , 210        , 1        , 4      , "Полная" , "Рефрижератор", 10.     , 209.   ),
            Order(9       , false     , 40        , 80         , 4        , 1      , "Полная" , "Рефрижератор", 40.     , 6.     ),
            Order(10      , false     , 175       , 185        , 1        , 4      , "Полная" , "Рефрижератор", 10.     , 184.   )
        });
    }

    void SetUpDistances(Distances& distances) {
        for (size_t i = 1; i <= 4; ++i) {
            for (size_t j = 1; j <= 4; ++j) {
                if (i != j) {
                    int diff = std::max(i, j) - std::min(i, j);
                    distances.dists[{i, j}] = diff * 10.;
                }
            }
        }
    }

    void SetUpSolution(solution_t& solution) {
        solution = {{
            {8, 4, 5, 6, 7}, // first truck orders positions
            {0, 2, 3, 9}     // second truck orders positions
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
        data_.cities_count = 4;


        SetUpSolution(expected_);
    }

    // void TearDown() override {}
};

TEST_F(TrickyDataTest, FlowSolverTestBasic) {
    FlowSolver solver;
    solver.SetData(data_);
    auto model = solver.CreateModel();
    solution_t solution = solver.Solve(model);

    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for TrickyData)";
}

TEST_F(TrickyDataTest, WeightedSolverTestBasic) {
    WeightedCitiesSolver solver;
    solver.SetData(data_);
    solution_t solution = solver.Solve();

    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for TrickyData";
}

// cities_ws = 0, time_boundary = 0 => nothing change
TEST_F(TrickyDataTest, WeightedSolverTestTimeBoundary1) {
    FreeMovementWeightsVectors cities_ws;
    unsigned int time_boundary = 0;

    WeightedCitiesSolver solver;
    solver.SetData(data_, time_boundary, cities_ws);
    solution_t solution = solver.Solve();

    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for TrickyData (cities_ws = 0, time_boundary = 0 => suppose to change nothing)";
}

// cities_ws = 0, time_boundary is big => nothing change
TEST_F(TrickyDataTest, WeightedSolverTestTimeBoundary2) {
    FreeMovementWeightsVectors cities_ws;
    unsigned int time_boundary = 1000;

    WeightedCitiesSolver solver;
    solver.SetData(data_, time_boundary, cities_ws);
    solution_t solution = solver.Solve();

    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for TrickyData (cities_ws = 0, time_boundary is big => suppose to change nothing)";
}

TEST_F(TrickyDataTest, BatchSolverFlowTestOneBatch) {
    std::shared_ptr<WeightedCitiesSolver> solver = std::make_shared<WeightedCitiesSolver>();
    BatchSolver batch_solver(std::move(solver));

    // big time bound
    solution_t solution = batch_solver.Solve(data_, 1000);
    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for TrickyData";

    // time bound = max(for order in orders {order.finish_time})
    solution = batch_solver.Solve(data_, 210);
    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for TrickyData";

    // time bound = max(for order in orders {order.start_time}) + 1
    solution = batch_solver.Solve(data_, 201);
    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for TrickyData (time bound = max(for order in orders {order.start_time}) + 1 still suppose to produce only one batch)";
}

TEST_F(TrickyDataTest, BatchSolverAssignmentTestOneBatch) {
    std::shared_ptr<ChainSolver> solver = std::make_shared<ChainSolver>(-1e9, 5);
    ASSERT_EQ(5, expected_.orders_by_truck_pos[0].size());

    BatchSolver batch_solver(std::move(solver));

    // big time bound
    solution_t solution = batch_solver.Solve(data_, 1000);
    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for TrickyData";

    // time bound = max(for order in orders {order.finish_time})
    solution = batch_solver.Solve(data_, 210);
    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for TrickyData";

    // time bound = max(for order in orders {order.start_time}) + 1
    solution = batch_solver.Solve(data_, 201);
    EXPECT_EQ(expected_.orders_by_truck_pos, solution.orders_by_truck_pos) << "Suppose to be ideal solution for TrickyData (time bound = max(for order in orders {order.start_time}) + 1 still suppose to produce only one batch)";
}

TEST_F(TrickyDataTest, CheckerTest) {
    Checker checker(data_);
    checker.SetSolution(expected_);

    auto revenue_raw = checker.Check();
    ASSERT_TRUE(revenue_raw.has_value());
    EXPECT_DOUBLE_EQ(2., revenue_raw.value());
}
project(month_schedule_test LANGUAGES CXX)

enable_testing()
add_executable(main_test 
    main.cpp
)

include(GoogleTest)
target_link_libraries(main_test
    GTest::GTest 
    GTest::Main
)
gtest_discover_tests(main_test)
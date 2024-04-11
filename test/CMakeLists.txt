project(month_schedule_test LANGUAGES CXX)

add_executable(main_test 
    main.cpp
)
target_link_libraries(main_test gtest_main)

enable_testing()
include(GoogleTest)
gtest_discover_tests(main_test)
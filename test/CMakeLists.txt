project(month_schedule_test LANGUAGES CXX)

# dont wanna a lot of debug output even in Debug
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    remove_definitions(-DDEBUG_MODE)
endif()

include_directories(../include)

set(test_sources ${sources})
list(TRANSFORM test_sources PREPEND "../")
add_executable(main_test 
    main.cpp
    solvers_test_small.cpp
    solvers_test_tricky.cpp
    ${test_sources}
)
target_link_libraries(main_test 
    gtest_main
    highs::highs
    OpenXLSX::OpenXLSX
)
# ./test/main_test --gtest_filter=""

enable_testing()
include(GoogleTest)
gtest_discover_tests(main_test)
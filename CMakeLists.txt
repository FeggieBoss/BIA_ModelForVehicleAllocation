cmake_minimum_required(VERSION 3.1)
project(month_schedule LANGUAGES CXX)

if (NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL Debug)
    set(CMAKE_BUILD_TYPE Debug) 
    add_definitions(-DDEBUG_MODE)
endif() # DCMAKE_BUILD_TYPE=Release

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast")

include_directories(include)
include_directories(HiGHs)

set(sources
    src/xlsx_cell.cpp
    src/params.cpp
    src/trucks.cpp
    src/orders.cpp
    src/distances.cpp
    src/data.cpp
    src/checker.cpp
    src/solver.cpp
    src/honest_solver.cpp
    src/weighted_cities_solver.cpp
    src/batch_solver.cpp
    src/pre_solver.cpp
)
add_executable(main
    src/main.cpp
    ${sources}
)

# HiGHs
set(HIGHS_DIR ./HiGHs/lib/cmake/highs)
find_package(HIGHS REQUIRED)
find_package(Threads REQUIRED)
target_link_libraries(main highs::highs)

# OpenXLSX
add_subdirectory(OpenXLSX)
target_link_libraries(main OpenXLSX::OpenXLSX)

# GoogleTests
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
FetchContent_MakeAvailable(googletest)

add_subdirectory(test)
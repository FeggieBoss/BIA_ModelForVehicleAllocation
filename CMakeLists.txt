cmake_minimum_required(VERSION 3.1)
project(month_schedule LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug) 
endif() # cmake .. -DCMAKE_BUILD_TYPE=Release

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast")

set(HIGHS_DIR ./lib/cmake/highs)

find_package(HIGHS REQUIRED)
find_package(Threads REQUIRED)

add_subdirectory(OpenXLSX)

include_directories(include)
add_executable(main 
    src/main.cpp 
    src/xlsx_cell.cpp
    src/params.cpp
    src/trucks.cpp
    src/orders.cpp
    src/distances.cpp
    src/data.cpp
    src/checker.cpp
)
target_link_libraries(main highs::highs)
target_link_libraries(main OpenXLSX::OpenXLSX)
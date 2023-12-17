project(month_schedule LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

set(HIGHS_DIR /home/feggie/MyData/BIA/lib/cmake/highs)

find_package(HIGHS REQUIRED)
find_package(Threads REQUIRED)

add_subdirectory(OpenXLSX)

add_executable(main main.cpp parse.cpp)
target_link_libraries(main highs::highs)
target_link_libraries(main OpenXLSX::OpenXLSX)
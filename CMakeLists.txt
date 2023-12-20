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

add_executable(main main.cpp parse.cpp model.cpp)
target_link_libraries(main highs::highs)
target_link_libraries(main OpenXLSX::OpenXLSX)
cmake_minimum_required(VERSION 3.16)
project(plant_controller CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wextra -Wpedantic)

find_package(SQLite3 REQUIRED)

add_library(watering_core STATIC
    src/watering_controller.cpp
    src/config.cpp
)
target_include_directories(watering_core PUBLIC include)

add_executable(plant_controller
    src/main.cpp
    src/mcp3008.cpp
    src/gpio_relay.cpp
    src/data_logger.cpp
    src/simulation.cpp
)
target_link_libraries(plant_controller PRIVATE watering_core SQLite::SQLite3)

enable_testing()
add_executable(test_controller tests/test_controller.cpp)
target_link_libraries(test_controller PRIVATE watering_core)
add_test(NAME controller COMMAND test_controller)

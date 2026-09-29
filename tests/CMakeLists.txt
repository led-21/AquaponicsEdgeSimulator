cmake_minimum_required(VERSION 3.16)
project(AquaponicsEdgeSimulator_Tests C)

enable_testing()

set(TEST_SRCS
    test_aquaponics_simulator.c
    ../main/simulator.c
    ../main/telemetry.c
)

add_executable(unit_tests ${TEST_SRCS})

target_include_directories(unit_tests PRIVATE
    ../main/include
    /usr/include/cjson
)

find_package(cJSON QUIET)

if(TARGET cjson)
    target_link_libraries(unit_tests PRIVATE cjson m)
else()
    target_link_libraries(unit_tests PRIVATE cjson m)
endif()

add_test(NAME unit_tests COMMAND unit_tests)

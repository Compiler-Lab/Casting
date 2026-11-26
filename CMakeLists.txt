cmake_minimum_required(VERSION 3.10)

project(Casting VERSION 0.1.0 LANGUAGES C CXX)



set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected, defaulting to Debug (Symbols enabled)")
    set(CMAKE_BUILD_TYPE "Debug")
endif()

add_library(casting_core STATIC
    lib/diagnostic.cpp
    lib/tokenize.cpp
    lib/parser.cpp
    lib/codegen.cpp
    lib/type.cpp
    lib/utils.cpp
)

target_include_directories(casting_core PUBLIC "${CMAKE_SOURCE_DIR}/include")


add_executable(Casting
    Main.cpp
)

target_link_libraries(Casting PRIVATE casting_core)
enable_testing()

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/test/*.c")

set(TEST_BINS "")
foreach(test_src ${TEST_SOURCES})
    get_filename_component(test_name "${test_src}" NAME_WE)
    set(test_asm "${CMAKE_CURRENT_BINARY_DIR}/test/${test_name}.s")
    set(test_bin "${CMAKE_CURRENT_BINARY_DIR}/test/${test_name}")

    add_custom_command(
        OUTPUT "${test_bin}"
        BYPRODUCTS "${test_asm}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/test"
        COMMAND bash -o pipefail -c
            "\"${CMAKE_C_COMPILER}\" -I\"${CMAKE_SOURCE_DIR}/test\" -o- -E -P -C \"${test_src}\" | \"$<TARGET_FILE:Casting>\" -o \"${test_asm}\" -"
        COMMAND "${CMAKE_C_COMPILER}" -o "${test_bin}" "${test_asm}" -xc "${CMAKE_SOURCE_DIR}/test/common"
        DEPENDS Casting "${test_src}" "${CMAKE_SOURCE_DIR}/test/common" "${CMAKE_SOURCE_DIR}/test/test.h"
        COMMENT "Building C test ${test_name} with Casting"
        VERBATIM
    )

    list(APPEND TEST_BINS "${test_bin}")
    add_test(NAME "test_${test_name}" COMMAND "${test_bin}")
endforeach()

add_custom_target(casting_tests ALL DEPENDS ${TEST_BINS})

add_test(NAME "test_driver"
         COMMAND "${CMAKE_SOURCE_DIR}/test/driver.sh" "$<TARGET_FILE:Casting>")

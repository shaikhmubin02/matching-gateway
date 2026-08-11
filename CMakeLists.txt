cmake_minimum_required(VERSION 3.20)
project(matching-gateway LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GW_BUILD_TESTS "Build unit tests" ON)
option(GW_BUILD_BENCH "Build benchmarks" ON)

add_library(mdf
  src/mdf/protocol.cpp
  src/mdf/order_book.cpp
  src/mdf/seq_tracker.cpp
  src/mdf/recovery.cpp
  src/mdf/recorder.cpp
  src/mdf/replay.cpp
  src/mdf/feed_handler.cpp
)
target_include_directories(mdf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(lob third_party/lob/Orderbook.cpp)
target_include_directories(lob PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lob)

add_library(gw
  src/order_protocol.cpp
  src/gateway.cpp
)
target_include_directories(gw PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lob
)
target_link_libraries(gw PUBLIC mdf lob)

add_executable(gw_demo src/main.cpp)
target_link_libraries(gw_demo PRIVATE gw)

include(FetchContent)

if(GW_BUILD_TESTS)
  set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
  )
  FetchContent_MakeAvailable(googletest)
  enable_testing()
  add_executable(gw_tests tests/test_gateway.cpp)
  target_link_libraries(gw_tests PRIVATE gw GTest::gtest_main)
  include(GoogleTest)
  gtest_discover_tests(gw_tests)
endif()

if(GW_BUILD_BENCH)
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.1
  )
  FetchContent_MakeAvailable(googlebenchmark)
  add_executable(gw_bench bench/bench_gateway.cpp)
  target_link_libraries(gw_bench PRIVATE gw benchmark::benchmark)
endif()

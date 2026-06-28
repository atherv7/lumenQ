cmake_minimum_required(VERSION 3.15)
project(lumenQ C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(
  -Wall
  -Wextra
  -Werror
  -pedantic
  -Wshadow
  -Wpointer-arith
  -Wcast-align
  -Wstrict-prototypes
)

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose build type" FORCE)
  add_compile_options(
    -O3
    -march=native
    -fomit-frame-pointer
  )
endif()

find_library(LIB_RT rt REQUIRED)
find_library(LIB_PTHREAD pthread REQUIRED)

add_library(lumenQ STATIC
  src/shm_utils.c
  src/producer.c
  src/consumer.c
)

target_include_directories(lumenQ PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(lumenQ PRIVATE ${LIB_RT} ${LIB_PTHREAD})

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/reader_app.c")
  add_executable(reader_app_example examples/reader_app.c)
  target_link_libraries(reader_app_example PRIVATE lumenQ)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/writer_app.c")
  add_executable(writer_app_example examples/writer_app.c)
  target_link_libraries(writer_app_example PRIVATE lumenQ)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bench/latency_reporter.c")
  add_executable(bench_latency bench/latency_reporter.c)
  target_link_libraries(bench_latency PRIVATE lumenQ)
endif()

enable_testing()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/race_condition_test.c")
  add_executable(test_race_condition tests/race_condition_test.c)
  target_link_libraries(test_race_condition PRIVATE lumenQ)

  target_compile_definitions(test_race_condition PRIVATE "NDEBUG_DISABLED_FOR_TESTING")
  target_compile_options(test_race_condition PRIVATE -UNDEBUG)

  target_compile_options(test_race_condition PRIVATE -fsanitize=thread)
  target_link_options(test_race_condition PRIVATE -fsanitize=thread)

  add_test(NAME RaceConditionTest COMMAND test_race_condition)
endif()

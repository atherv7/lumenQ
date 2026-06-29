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
  src/tuning.c
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

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bench/benchmark_latency.c")
  add_executable(bench_latency bench/benchmark_latency.c)
  target_link_libraries(bench_latency PRIVATE lumenQ)
endif()

enable_testing()

file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.c")

foreach(SOURCE_FILE ${TEST_SOURCES})
  get_filename_component(TEST_NAME ${SOURCE_FILE} NAME_WE)
  set(TARGET_NAME "${TEST_NAME}")
  string(MAKE_C_IDENTIFIER "${TEST_NAME}" TEST_TITLE)

  add_executable(${TARGET_NAME} ${SOURCE_FILE})
  target_link_libraries(${TARGET_NAME} PRIVATE lumenQ)

  target_compile_definitions(${TARGET_NAME} PRIVATE "NDEBUG_DISABLED_FOR_TESTING")
  target_compile_options(${TARGET_NAME} PRIVATE -UNDEBUG)

  target_compile_options(${TARGET_NAME} PRIVATE -fsanitize=thread)
  target_link_options(${TARGET_NAME} PRIVATE -fsanitize=thread)

  add_test(NAME ${TEST_TITLE} COMMAND ${TARGET_NAME})
endforeach()

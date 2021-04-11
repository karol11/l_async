cmake_minimum_required (VERSION 3.8)

project ("l_async")

set(CMAKE_CXX_STANDARD 17)
add_definitions(-D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)

include_directories (
    "include"
    "tests"
    "docs"
)

add_executable (l_async

    "include/l_async.h"

    "tests/gunit.h"
    "tests/gunit.cpp"
    "tests/single_thread_executor.h"
    "tests/loop_test.cpp"
    "tests/slot_test.cpp"

    "docs/sync_fs_scan_problem.h"
    "docs/sync_fs_scan_solution.cpp"
    "docs/sync_fs_scan_test.cpp"

    "docs/async_fs_scan_problem.h"
    "docs/async_fs_scan_solution.cpp"
    "docs/async_fs_scan_test.cpp"

    "examples/loop_example.cpp"
    "examples/result_example.cpp"
    "examples/slot_example.cpp"
)

cmake_minimum_required (VERSION 3.12)

set(BENCHMARK_ENABLE_TESTING NO)

include(FetchContent)
FetchContent_Declare(
    googlebenchmark
    # v1.8.0
    URL  https://github.com/google/benchmark/archive/2dd015dfef425c866d9a43f2c67d8b52d709acb6.zip
)
FetchContent_MakeAvailable(googlebenchmark)

add_executable(benchmark_test
    "sync.cpp"
    "map.cpp"
)
target_link_libraries(benchmark_test PRIVATE
    automerge
    benchmark::benchmark
)

# Link Shlwapi to the project
if ("${CMAKE_SYSTEM_NAME}" MATCHES "Windows")
    target_link_libraries(benchmark_test PRIVATE Shlwapi)
endif()

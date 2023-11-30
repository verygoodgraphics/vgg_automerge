cmake_minimum_required (VERSION 3.12)

project ("vgg_automerge")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")

# sub-project
add_subdirectory ("vgg_contrib/json")
add_subdirectory ("vgg_contrib/picosha2")
add_subdirectory ("vgg_contrib/sdefl")
add_subdirectory ("automerge")

if (EMSCRIPTEN)
else()
    # add test targets
    include(CTest)
    if (BUILD_TESTING)
        enable_testing()
        add_subdirectory("test")
    endif()

    # add benchmark test
    add_subdirectory("benchmark")
endif()

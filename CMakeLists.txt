cmake_minimum_required (VERSION 3.12)

project ("vgg_automerge")

set(CMAKE_CXX_STANDARD 17)

# sub-project
add_subdirectory ("vgg_contrib/json")
add_subdirectory ("vgg_contrib/picosha2")
add_subdirectory ("vgg_contrib/sdefl")
add_subdirectory ("automerge")

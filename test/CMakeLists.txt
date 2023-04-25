cmake_minimum_required (VERSION 3.12)

if (EMSCRIPTEN)
else()
	include(FetchContent)
	FetchContent_Declare(
	  googletest
	  URL  https://github.com/google/googletest/archive/b796f7d44681514f58a683a3a71ff17c94edb0c1.zip
	)
	# For Windows: Prevent overriding the parent project's compiler/linker settings
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
	FetchContent_MakeAvailable(googletest)

	enable_testing()

	add_executable(unit_test
	  hello_test.cpp
	)
	target_link_libraries(unit_test
	  GTest::gtest_main
	)

	include(GoogleTest)
	gtest_discover_tests(unit_test)
endif()

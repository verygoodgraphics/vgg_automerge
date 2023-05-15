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
		"hello_test.cpp"
		"automerge_test.cpp"
	)
	target_link_libraries(unit_test PRIVATE
		automerge
		GTest::gtest_main
	)
	add_custom_command(TARGET unit_test POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/test/fuzz-crashers" "${CMAKE_CURRENT_BINARY_DIR}/fuzz-crashers"
		COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/test/fixtures" "${CMAKE_CURRENT_BINARY_DIR}/fixtures"
	)

	include(GoogleTest)
	gtest_discover_tests(unit_test)
endif()

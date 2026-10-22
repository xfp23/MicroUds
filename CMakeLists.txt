cmake_minimum_required(VERSION 3.15)
project(MICROUDS VERSION 0.1 LANGUAGES C)

# Build options
option(BUILD_SHARED_LIBS "Build MICROUDS as shared library" OFF)
option(MICROUDS_BUILD_TESTS "Build tests (if tests exist)" OFF)

# Collect sources
file(GLOB_RECURSE MICROUDS_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Library target
add_library(MICROUDS ${MICROUDS_SOURCES})

target_include_directories(MICROUDS
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
)

set_target_properties(MICROUDS PROPERTIES
	C_STANDARD 99
	C_STANDARD_REQUIRED ON
	POSITION_INDEPENDENT_CODE OFF
)

# Compiler warnings (adjust for your toolchain)
if(MSVC)
	target_compile_options(MICROUDS PRIVATE /W4 /wd4996)
else()
	target_compile_options(MICROUDS PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Optional example: build if examples/main.c exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/main.c")
	add_executable(MICROUDS_example examples/main.c)
	target_link_libraries(MICROUDS_example PRIVATE MICROUDS)
	target_include_directories(MICROUDS_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()

# Optional tests: user can put tests under tests/ and enable MICROUDS_BUILD_TESTS
if(MICROUDS_BUILD_TESTS)
	enable_testing()
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
		# Example test targets (uncomment and adapt to your tests):
		# add_executable(test_basic tests/test_basic.c)
		# target_link_libraries(test_basic PRIVATE MICROUDS)
		# add_test(NAME basic COMMAND test_basic)
		message(STATUS "MICROUDS_BUILD_TESTS enabled — add test targets under 'tests/' in this CMakeLists.txt")
	else()
		message(STATUS "MICROUDS_BUILD_TESTS enabled but no 'tests/' folder found")
	endif()
endif()

# Install rules (simple)
include(GNUInstallDirs)
install(TARGETS MICROUDS
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Helpful configure-time messages
message(STATUS "Project: ${PROJECT_NAME} ${PROJECT_VERSION}")
message(STATUS "Source files: ${MICROUDS_SOURCES}")
message(STATUS "Build shared libs: ${BUILD_SHARED_LIBS}")
message(STATUS "Build tests: ${MICROUDS_BUILD_TESTS}")

cmake_minimum_required(VERSION 3.13)
project(queue_pool C)

# Use C99
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Option: build example
option(BUILD_EXAMPLES "Build example programs" ON)

# Add library
add_library(queue_pool STATIC
    src/Queue.c
    src/Queue.h
    src/Queue_types.h
)

# Public include directory (so users can include "Queue.h")
target_include_directories(queue_pool
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

# Compiler warnings (optional)
if(MSVC)
    target_compile_options(queue_pool PRIVATE /W4 /wd4996)
else()
    target_compile_options(queue_pool PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Build example if requested
if(BUILD_EXAMPLES)
    add_subdirectory(example)
endif()

# Install rules (optional)
include(GNUInstallDirs)
install(TARGETS queue_pool
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

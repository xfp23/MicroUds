cmake_minimum_required(VERSION 3.15)

project(MICROUDS LANGUAGES C)

# Collect library sources
file(GLOB_RECURSE MICROUDS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Server/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Client/src/*.c"
)

# Library target
add_library(MICROUDS ${MICROUDS_SOURCES})

target_include_directories(MICROUDS
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Server/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Client/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(MICROUDS PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE OFF
)

# Compiler warnings
if(MSVC)
    target_compile_options(MICROUDS PRIVATE /W4 /wd4996)
else()
    target_compile_options(MICROUDS PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install rules
include(GNUInstallDirs)

install(TARGETS MICROUDS
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY Server/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY Client/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Configure summary
message(STATUS "Project: ${PROJECT_NAME} ${PROJECT_VERSION}")
message(STATUS "Source files: ${MICROUDS_SOURCES}")
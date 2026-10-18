cmake_minimum_required(VERSION 4.3)

project(MICROUDSC)

file(GLOB_RECURSE MICROUDS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

add_library(MICROUDS STATIC
    ${MICROUDS_SOURCES}
)


target_include_directories(MICROUDS PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(MICROUDS PRIVATE
    "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)
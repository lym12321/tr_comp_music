cmake_minimum_required(VERSION 3.22)
project(music VERSION 0.0.1 LANGUAGES CXX)

file(GLOB_RECURSE SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
)

add_library(music STATIC ${SOURCES})
target_include_directories(music
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/internal
)

target_link_libraries(music PRIVATE bsp components::utils)

set_target_properties(music PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED YES
        POSITION_INDEPENDENT_CODE NO
)

add_library(components::music ALIAS music)

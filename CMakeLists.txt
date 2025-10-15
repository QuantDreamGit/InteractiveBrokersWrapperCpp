cmake_minimum_required(VERSION 3.14)
project(IBWrapper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE WRAPPER_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

add_library(IBWrapper STATIC ${WRAPPER_SOURCES})

target_include_directories(IBWrapper PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(IBWrapper PUBLIC ibapi)

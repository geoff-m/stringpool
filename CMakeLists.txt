cmake_minimum_required(VERSION 3.20)
project(stringpool)

set(CMAKE_CXX_STANDARD 20)

add_library(stringpool STATIC
        src/stringpool.cpp
        include/stringpool/stringpool.h)

target_include_directories(stringpool PUBLIC include)

find_package(Boost CONFIG REQUIRED COMPONENTS unordered)
find_package(xxHash CONFIG REQUIRED)

target_link_libraries(stringpool
        PRIVATE xxHash::xxhash
        PRIVATE Boost::unordered)

enable_testing()
find_package(GTest CONFIG REQUIRED)
add_subdirectory(test)
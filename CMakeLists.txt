cmake_minimum_required(VERSION 3.20)
project(stringpool)
include(cmake/asan.cmake)
set(CMAKE_CXX_STANDARD 20)

option(STRINGPOOL_REFCOUNT_ENABLE
        "Whether stringpool should reference-count its contents. Disabling will make the cache grow-only and may slightly boost performance in debug builds, with even less of a boost in optimized builds."
        ON)

add_library(stringpool STATIC
        src/stringpool.cpp
        include/stringpool/stringpool.h
        src/include/hash.h
        src/hash.cpp
        src/statistics.cpp
        src/string_handle.cpp
        src/pack_utils.cpp
        src/char_iterator.cpp
        src/reverse_char_iterator.cpp
        src/tree_walker.cpp
        src/reverse_tree_walker.cpp)

if (STRINGPOOL_REFCOUNT_ENABLE)
    target_compile_definitions(stringpool PUBLIC STRINGPOOL_REFCOUNT_ENABLE=1)
endif()

target_include_directories(stringpool PUBLIC include)

find_package(xxHash CONFIG REQUIRED)

target_link_libraries(stringpool
        PRIVATE xxHash::xxhash)

enable_testing()
find_package(GTest CONFIG REQUIRED)
add_subdirectory(test)
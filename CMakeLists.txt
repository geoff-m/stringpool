cmake_minimum_required(VERSION 3.20)
project(stringpool)
include(cmake/asan.cmake)
set(CMAKE_CXX_STANDARD 20)

option(STRINGPOOL_REFCOUNT_ENABLE
        "Whether stringpool should reference-count its contents. Disabling will make the cache grow-only."
        ON)

option(STRINGPOOL_TEST_ENABLE
        "Whether stringpool should build tests."
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
        src/reverse_tree_walker.cpp
        src/default_allocator.cpp
        src/include/default_allocator.h
        src/chunk_iterator.cpp)

if (STRINGPOOL_REFCOUNT_ENABLE)
    target_compile_definitions(stringpool PUBLIC STRINGPOOL_REFCOUNT_ENABLE=1)
endif()

target_include_directories(stringpool PUBLIC include)

find_package(xxHash CONFIG REQUIRED)

target_link_libraries(stringpool
        PRIVATE xxHash::xxhash)

if (STRINGPOOL_TEST_ENABLE)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    add_subdirectory(test)
endif()

include(GNUInstallDirs)
set_target_properties(stringpool PROPERTIES PUBLIC_HEADER include/stringpool/stringpool.h)
install(TARGETS stringpool
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/stringpool
)
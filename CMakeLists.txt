cmake_minimum_required(VERSION 3.20)
project(stringpool)
set(CMAKE_CXX_STANDARD 20)

option(STRINGPOOL_REFCOUNT_ENABLE
        "Whether stringpool should reference-count its contents. Disabling will make the cache grow-only."
        ON)

option(STRINGPOOL_TEST_ENABLE
        "Whether stringpool should build tests."
        ON)

option(STRINGPOOL_ASAN_ENABLE
        "Whether stringpool should build with Address Sanitizer"
        OFF)

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
        src/chunk_iterator_forward.cpp
        src/chunk_iterator_backward.cpp)

if (STRINGPOOL_REFCOUNT_ENABLE)
    target_compile_definitions(stringpool PUBLIC STRINGPOOL_REFCOUNT_ENABLE=1)
endif()

if(STRINGPOOL_ENABLE_ASAN)
    target_compile_options(stringpool PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(stringpool PRIVATE -fsanitize=address)
endif()

target_include_directories(stringpool
PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>  
    $<INSTALL_INTERFACE:include/>
)

target_compile_options(stringpool PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wall -Wextra -Wpedantic -Wconversion -Werror>
)

# This propagates C++ standard 20 (at least) to targets that link to this.
target_compile_features(stringpool INTERFACE cxx_std_20)

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
        EXPORT stringpool-export
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT stringpool-export
        FILE stringpool-export.cmake
        NAMESPACE stringpool::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/stringpool
)

install(FILES cmake/stringpoolConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/stringpool
)
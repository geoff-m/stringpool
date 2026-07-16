cmake_minimum_required(VERSION 3.20)

add_executable(test-stringpool
        src/BasicTests.cpp
        src/StrcmpTests.cpp
        src/MemcmpTests.cpp
        src/HashTests.cpp
        src/include/Utility.h
        src/Utility.cpp
        src/GrowTableTests.cpp
        src/RaceTests.cpp
        src/CrossPool.cpp
        src/StatisticsTests.cpp
        src/VisitTests.cpp
        src/ToStringTests.cpp
        src/CharIteratorTests.cpp
        src/ReverseCharIteratorTests.cpp
        src/AllocatorTests.cpp
        src/MoveTests.cpp
        src/AssignTests.cpp
        src/FormatTests.cpp
        src/ChunkIteratorTests.cpp
        src/ReverseChunkIteratorTests.cpp
        src/ReadmeTests.cpp
        src/OperatorEqualsTest.cpp
        src/MapTests.cpp
        src/UnorderedMapTests.cpp
        src/VariadicConcatTests.cpp)

target_include_directories(test-stringpool PRIVATE src/include)

target_link_libraries(test-stringpool
        PRIVATE GTest::gtest_main
        PRIVATE stringpool)

include(GoogleTest)
gtest_discover_tests(test-stringpool)
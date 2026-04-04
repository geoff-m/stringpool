cmake_minimum_required(VERSION 3.20)

add_executable(test-stringpool
    src/BasicTests.cpp
        src/StrcmpTests.cpp
        src/MemcmpTests.cpp
        src/HashTests.cpp)

target_link_libraries(test-stringpool
    PRIVATE GTest::gtest_main
    PRIVATE stringpool)

include(GoogleTest)
gtest_discover_tests(test-stringpool)
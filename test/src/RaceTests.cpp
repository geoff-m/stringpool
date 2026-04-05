#include <cstring>
#include <gtest/gtest.h>
#include "stringpool/stringpool.h"
#include <thread>

using namespace stringpool;

void skipIfNoParallelism() {
    if (std::thread::hardware_concurrency() < 2) {
        GTEST_SKIP() << "Skipping test because there's no hardware concurrency";
    }
}

TEST(Race, InternShortAtom) {
    skipIfNoParallelism();
    std::vector<std::thread> threads;
    constexpr auto TOTAL_INTERNS = 10000;
    pool pool(1000, 10);
    const auto threadCount = std::thread::hardware_concurrency();
    for (int i=0; i < threadCount; ++i)
        threads.emplace_back([&] {
            constexpr auto bufferSize = 8;
            char buf[bufferSize] = {};
            for (int string = 0; string < TOTAL_INTERNS / threadCount; ++string) {
                snprintf(buf, bufferSize, "%d", string);
                pool.intern(buf);
            }
        });
    for (int i=0; i < threadCount; ++i)
        threads[i].join();
}

TEST(Race, InternShortConcat) {
    skipIfNoParallelism();
    std::vector<std::thread> threads;
    constexpr auto TOTAL_INTERNS = 10000;
    pool pool(1000, 10);
    const auto threadCount = std::thread::hardware_concurrency();
    const auto INTERNS_PER_THREAD = TOTAL_INTERNS / threadCount;
    for (int i=0; i < threadCount; ++i)
        threads.emplace_back([&] {
            constexpr auto bufferSize = 8;
            char buf[bufferSize] = {};
            for (int string = 0; string < INTERNS_PER_THREAD; ++string) {
                snprintf(buf, bufferSize, "%d", string);
                pool.intern(buf);
            }
            char leftBuf[bufferSize] = {};
            char rightBuf[bufferSize] = {};
            for (int string = 0; string <INTERNS_PER_THREAD ; ++string) {
                const auto leftInt = (string << 1) + 1;
                const auto rightInt = string << 1;
                if (leftInt >= INTERNS_PER_THREAD)
                    break;
                snprintf(leftBuf, bufferSize, "%d", leftInt);
                snprintf(rightBuf, bufferSize, "%d", rightInt);
                auto leftIntern = pool.intern(leftBuf);
                auto rightIntern = pool.intern(rightBuf);
                pool.concat(leftIntern, rightIntern);
            }
        });
    for (int i=0; i < threadCount; ++i)
        threads[i].join();
}

TEST(Race, InternLongConcat) {
    skipIfNoParallelism();
    std::vector<std::thread> threads;
    constexpr auto TOTAL_INTERNS = 10000;
    pool pool(1000, 10);
    const auto threadCount = std::min(4u, std::thread::hardware_concurrency());
    const auto INTERNS_PER_THREAD = TOTAL_INTERNS / threadCount;
    for (int i=0; i < threadCount; ++i)
        threads.emplace_back([&] {
            const auto atomFormat = "%d...................";
            constexpr auto bufferSize = 30;
            char buf[bufferSize] = {};
            for (int string = 0; string < INTERNS_PER_THREAD; ++string) {
                // 19 periods
                snprintf(buf, bufferSize, atomFormat, string);
                pool.intern(buf);
            }
            char leftBuf[bufferSize] = {};
            char rightBuf[bufferSize] = {};
            for (int string = 0; string <INTERNS_PER_THREAD ; ++string) {
                const auto leftInt = (string << 1) + 1;
                const auto rightInt = string << 1;
                if (leftInt >= INTERNS_PER_THREAD)
                    break;
                snprintf(leftBuf, bufferSize, atomFormat, leftInt);
                snprintf(rightBuf, bufferSize, atomFormat, rightInt);
                auto leftIntern = pool.intern(leftBuf);
                auto rightIntern = pool.intern(rightBuf);
                pool.concat(leftIntern, rightIntern);
            }
        });
    for (int i=0; i < threadCount; ++i)
        threads[i].join();
}
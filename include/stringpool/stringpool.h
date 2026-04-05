#pragma once
#include <cstddef>
#include <deque>
#include <list>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace stringpool {
    struct allocator {
        char* allocate(size_t size);

        void deallocate(char* ptr, size_t size);

        char* reallocate(char* ptr, size_t oldSize, size_t newSize);
    };

    class pool;

    class string_handle {
        friend class pool;
        pool* owner;
        size_t dataIndex; // todo: make this relative to base of data.
        // currently this breaks on realloc, since the data buffer moves.

        string_handle(pool* owner, size_t dataIndex);

        string_handle() = default;

        class tree_walker {
            // We assume this won't change during the lifetime of this object.
            // This is currently upheld by users of this class.
            char* baseAddress;

            size_t rootIndex;
            std::deque<size_t> toVisit;

        public:
            tree_walker(const pool& owner, size_t rootIndex);

            tree_walker(char* baseAddress, size_t rootIndex);

            [[nodiscard]] size_t get_next_bytes(char** bytes);
        };

        [[nodiscard]] static bool concat_equals_unsafe(char* entry, string_handle left, string_handle right);

        size_t copy_unsafe(char* destination, size_t destination_size) const;

    public:
        void visit_pieces(void (*callback)(char* piece, size_t pieceSize, void* state), void* state) const;

        [[nodiscard]] size_t size() const;

        [[nodiscard]] size_t length() const;

        size_t copy(char* destination, size_t size) const;


        [[nodiscard]] size_t hash() const;

        /**
         * Compares this string with the given one.
         * @param rhs The null-terminated string to compare to this one.
         * @return The sign of the difference of the first byte that differs, or zero if none differ.
         */
        [[nodiscard]] int strcmp(const char* rhs) const;

        /**
         * Compares this string with the given one.
         * @param rhs The string to compare to this one.
         * @return The sign of the difference of the first byte that differs, or zero if none differ.
         */
        [[nodiscard]] int strcmp(const string_handle& rhs) const;

        /**
         * Compares this string with the given one.
         * @param rhs The string to compare to this one.
         * @return The sign of the difference of the first byte that differs, or zero if none differ.
         */
        [[nodiscard]] int memcmp(const string_handle& rhs, size_t length) const;

        /**
         * Compares this string with the given one.
         * @param rhs The string to compare to this one.
         * @param length Compare only this many characters.
         * @return The sign of the difference of the first byte that differs, or zero if none differ.
         */
        [[nodiscard]] int memcmp(const char* rhs, size_t length) const;

        /**
         * Compares this string with the given one.
         * @param rhs The string to compare to this one.
         * @param length The length of the string argument.
         * @return True if and only if the strings are equal.
         */
        [[nodiscard]] bool equals(const char* rhs, size_t length) const;


        /**
         * Compares this string with the given one.
         * @param rhs The null-terminated string to compare to this one.
         * @return True if and only if the strings are equal.
         */
        [[nodiscard]] bool equals(const char* rhs) const;

        /**
         * Compares this string with the given one.
         * @param rhs The string to compare to this one.
         * @return True if and only if the strings are equal.
         */
        [[nodiscard]] bool equals(const string_handle& rhs) const;
    };

    class pool {
        char* data;
        size_t dataSize = 0;
        size_t dataCapacity;
        friend class string_handle;

        // Prevents concurrent changes to table contents.
        std::shared_mutex tableRwMutex;

        void updateDataSizeUnsafe(size_t newSize);

        // key: hash
        // value: list of strings having that hash
        std::unordered_map<size_t, std::list<string_handle>> table;

        class reader_lock {
            using TReaderLock = std::shared_lock<std::shared_mutex>;
            TReaderLock lock1;
            TReaderLock lock2;
            TReaderLock lock3;

        public:
            explicit reader_lock(TReaderLock&& heldLock)
                : lock1(std::move(heldLock)) {
            }

            reader_lock(TReaderLock&& unheldLock1, TReaderLock&& unheldLock2)
                : lock1(std::move(unheldLock1)), lock2(std::move(unheldLock2)) {
                std::lock(lock1, lock2);
            }

            reader_lock(TReaderLock&& unheldLock1, TReaderLock&& unheldLock2, TReaderLock&& unheldLock3)
                : lock1(std::move(unheldLock1)), lock2(std::move(unheldLock2)), lock3(std::move(unheldLock3)) {
                std::lock(lock1, lock2, lock3);
            }

            void unlock() {
                lock1.unlock();
                if (lock2.owns_lock())
                    lock2.unlock();
                if (lock3.owns_lock())
                    lock3.unlock();
            }
        };

        class writer_lock {
            using TWriterLock = std::unique_lock<std::shared_mutex>;
            TWriterLock lock1;
            TWriterLock lock2;

        public:
            explicit writer_lock(TWriterLock&& heldLock)
                : lock1(std::move(heldLock)) {
            }

            writer_lock(TWriterLock&& unheldLock1, TWriterLock&& unheldLock2)
                : lock1(std::move(unheldLock1)), lock2(std::move(unheldLock2)) {
                std::lock(lock1, lock2);
            }
        };

        class writer_lock_reader_locks {
            using TWriterLock = std::unique_lock<std::shared_mutex>;
            using TReaderLock = std::shared_lock<std::shared_mutex>;
            TWriterLock writeLock;
            TReaderLock readLock1;
            TReaderLock readLock2;

        public:
            writer_lock_reader_locks(TWriterLock&& unheldWriteLock)
                : writeLock(std::move(unheldWriteLock)) {
                writeLock.lock();
            }

            writer_lock_reader_locks(TWriterLock&& unheldWriteLock, TReaderLock&& unheldReadLock1)
                : writeLock(std::move(unheldWriteLock)), readLock1(std::move(unheldReadLock1)) {
                std::lock(writeLock, readLock1);
            }

            writer_lock_reader_locks(TWriterLock&& unheldWriteLock, TReaderLock&& unheldReadLock1,
                                     TReaderLock&& unheldReadLock2)
                : writeLock(std::move(unheldWriteLock)), readLock1(std::move(unheldReadLock1)),
                  readLock2(std::move(unheldReadLock2)) {
                std::lock(writeLock, readLock1, readLock2);
            }

            ~writer_lock_reader_locks() {
                writeLock.unlock();
                if (readLock1.owns_lock())
                    readLock1.unlock();
                if (readLock2.owns_lock())
                    readLock2.unlock();
            }
        };

        [[nodiscard]] static reader_lock lock_for_reading(pool& p) {
            return reader_lock(std::shared_lock(p.tableRwMutex));
        }

        [[nodiscard]] static reader_lock lock_for_reading(pool& p1, pool& p2) {
            if (&p1 == &p2)
                return lock_for_reading(p1);
            return reader_lock(
                std::shared_lock(p1.tableRwMutex, std::defer_lock),
                std::shared_lock(p2.tableRwMutex, std::defer_lock));
        }

        // All arguments must be distinct.
        [[nodiscard]] static reader_lock lock_distinct_for_reading(pool& p1, pool& p2) {
            return reader_lock(
                std::shared_lock(p1.tableRwMutex, std::defer_lock),
                std::shared_lock(p2.tableRwMutex, std::defer_lock));
        }

        // All arguments must be distinct.
        [[nodiscard]] static reader_lock lock_distinct_for_reading(pool& p1, pool& p2, pool& p3) {
            return reader_lock(
                std::shared_lock(p1.tableRwMutex, std::defer_lock),
                std::shared_lock(p2.tableRwMutex, std::defer_lock),
                std::shared_lock(p3.tableRwMutex, std::defer_lock));
        }

        // Arguments don't have to be distinct.
        [[nodiscard]] static reader_lock lock_for_reading(pool& p1, pool& p2, pool& p3) {
            const bool oneIsTwo = &p1 == &p2;
            const bool oneIsThree = &p1 == &p3;
            const bool twoIsThree = &p2 == &p3;
            if (oneIsTwo) {
                if (twoIsThree) {
                    return lock_for_reading(p1);
                } else {
                    if (oneIsThree) {
                        return lock_for_reading(p1);
                    } else {
                        return lock_distinct_for_reading(p1, p3);
                    }
                }
            } else {
                if (twoIsThree) {
                    return lock_distinct_for_reading(p1, p2);
                } else {
                    if (oneIsThree) {
                        return lock_distinct_for_reading(p1, p2);
                    } else {
                        return lock_distinct_for_reading(p1, p2, p3);
                    }
                }
            }
        }

        // Locks the first pool for writing and the second pool for reading.
        // All arguments must be distinct.
        [[nodiscard]] static writer_lock_reader_locks lock_distinct_for_reading_and_writing(pool& write, pool& read) {
            return writer_lock_reader_locks(
                std::unique_lock(write.tableRwMutex, std::defer_lock),
                std::shared_lock(read.tableRwMutex, std::defer_lock));
        }

        // Locks the first pool for writing and the second two pools for reading.
        // All arguments must be distinct.
        [[nodiscard]] static writer_lock_reader_locks lock_distinct_for_reading_and_writing(
            pool& write, pool& read1, pool& read2) {
            return writer_lock_reader_locks(
                std::unique_lock(write.tableRwMutex, std::defer_lock),
                std::shared_lock(read1.tableRwMutex, std::defer_lock),
                std::shared_lock(read2.tableRwMutex, std::defer_lock));
        }

        // Locks the first pool for writing and the second two pools for reading.
        // If any pools are the same, no pool will be locked more than once.
        // If the write argument is equal to either read argument, that pool
        // will be locked for writing.
        [[nodiscard]] static writer_lock_reader_locks lock_for_reading_and_writing(
            pool& write, pool& read1, pool& read2) {
            bool shouldLockRead1 = true;
            bool shouldLockRead2 = true;
            if (&write == &read1)
                shouldLockRead1 = false;
            if (&write == &read2)
                shouldLockRead2 = false;
            if (&read1 == &read2)
                shouldLockRead2 = false;
            if (shouldLockRead1) {
                if (shouldLockRead2) {
                    return lock_distinct_for_reading_and_writing(write, read1, read2);
                } else {
                    return lock_distinct_for_reading_and_writing(write, read1);
                }
            } else {
                if (shouldLockRead2) {
                    return lock_distinct_for_reading_and_writing(write, read2);
                } else {
                    return writer_lock_reader_locks(std::unique_lock(write.tableRwMutex, std::defer_lock));
                }
            }
        }

        [[nodiscard]] static writer_lock lock_for_writing(pool& p1) {
            return writer_lock(std::unique_lock(p1.tableRwMutex));
        }

        [[nodiscard]] static writer_lock lock_for_writing(pool& p1, pool& p2) {
            if (&p1 == &p2)
                return lock_for_writing(p1);
            return writer_lock(
                std::unique_lock(p1.tableRwMutex, std::defer_lock),
                std::unique_lock(p2.tableRwMutex, std::defer_lock));
        }

        // These functions are not thread-safe.
        char* add_atom_unsafe(const char* string, size_t size);

        string_handle insertConcatUnsafe(size_t hash, string_handle left, string_handle right);

        enum class InternResult {
            Success = 0,
            NeedWriterLock = 1
        };

        // Caller must hold reader (or writer) lock.
        InternResult do_intern_unsafe(size_t hash,
                                      const char* string,
                                      size_t size,
                                      bool haveWriterLock,
                                      string_handle& result);

        // Caller must hold reader (or writer) lock on this and left and right owners.
        InternResult concat_helper_unsafe(
            size_t hash,
            string_handle left,
            string_handle right,
            bool haveWriterLock,
            string_handle& result);

    public:
        pool(size_t initial_table_capacity, size_t initial_data_capacity);

        pool();

        ~pool();

        string_handle intern(const char* string);

        string_handle intern(const char* string, size_t size);

        string_handle concat(string_handle left, string_handle right);
    };
}

bool operator==(const stringpool::string_handle& lhs, const stringpool::string_handle& rhs);

bool operator!=(const stringpool::string_handle& lhs, const stringpool::string_handle& rhs);

template<>
struct std::hash<stringpool::string_handle> {
    std::size_t operator()(stringpool::string_handle const& s) const noexcept {
        return s.hash();
    }
};

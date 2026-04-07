#pragma once
#include <cstddef>
#include <deque>
#include <list>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <iterator>


namespace stringpool {
    class pool;
    class string_handle {
        friend class pool;
        const char* data;

#ifdef STRINGPOOL_TRACK_OWNERS
        pool* owner;
        string_handle(const char* data, pool* owner);
#else
        string_handle(const char* data);
#endif

        string_handle() = default;

        class tree_walker {
            // We assume this won't change during the lifetime of this object.
            // This is currently upheld by users of this class.
            const char* root;

            std::deque<const char*> toVisit;

        public:
            tree_walker();
            tree_walker(const char* root);

            [[nodiscard]] size_t get_next_bytes(const char** bytes);

            [[nodiscard]] bool operator==(const tree_walker&) const = default;
        };

        [[nodiscard]] static bool concat_equals(string_handle single, string_handle left, string_handle right);

        class char_iterator
        {
            tree_walker walker;
            const char* chunk;
            size_t chunkSize;
            size_t indexInChunk;
        public:
            using value_type = char;
            using difference_type = std::ptrdiff_t;

            char_iterator();
            char_iterator(const string_handle& sh);
            char_iterator(const char_iterator&) = default;

            char_iterator& operator=(const char_iterator&) = default;

            [[nodiscard]] value_type operator*() const;

            char_iterator& operator++();

            char_iterator operator++(int);

            [[nodiscard]] bool operator==(const char_iterator& other) const;
            [[nodiscard]] bool operator!=(const char_iterator& other) const;
        };
        static_assert(std::forward_iterator<char_iterator>);

    public:
        /**
         * Gets the length of the string represented by this instance.
         * Same as string_handle::length.
         * @return The length of this string in bytes.
         */
        [[nodiscard]] size_t size() const;

        /**
         * Gets the length of the string represented by this instance.
         * Same as string_handle::size.
         * @return The length of this string in bytes.
         */
        [[nodiscard]] size_t length() const;

        /**
         * Copies this string.
         * @param destination The place to write the data of this string.
         * @param size The maximum number of bytes to write. Must not exceed the length of this string.
         * @return The number of bytes written.
         */
        size_t copy(char* destination, size_t size) const;

        /**
         * Copies this instance to a new std::string.
         * @return An std::string representing this string.
         */
        [[nodiscard]] std::string to_string() const;

        /**
         * Computes a hash on this string.
         * @return A non-cryptographic hash of this string.
         */
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
         * @param rhs The string to compare to this one.
         * @return True if and only if the strings are equal.
         */
        [[nodiscard]] bool equals(std::string_view rhs) const;

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

        /**
         * Invokes a callback on this string in a possibly piecewise manner.
         * @param callback A callback that will receive the data of this string.
         * @param state An arbitrary parameter that will be passed to the callback unchanged.
         */
        void visit_chunks(void (*callback)(const char* chunk, size_t chunk_size, void* state), void* state) const;

        /**
         * Invokes a callback on this string in a possibly piecewise manner.
         * @param callback A callback that will receive the data of this string.
         * @param state An arbitrary parameter that will be passed to the callback unchanged.
         */
        void visit_chunks(void (*callback)(std::string_view chunk, void* state), void* state) const;

        /**
         * Gets an iterator pointing to the first character in this string.
         */
        [[nodiscard]] char_iterator begin() const;

        /**
         * Gets an iterator pointing one character past the last character in this string.
         */
        [[nodiscard]] char_iterator end() const;
    };

    class pool {
        std::vector<char*> data;
        size_t totalDataSize = 0;

        [[nodiscard]] char* add_buffer(size_t size);
        friend class string_handle;

        // Prevents concurrent changes to table contents.
        std::shared_mutex tableRwMutex;

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


        [[nodiscard]] static writer_lock lock_for_writing(pool& p1) {
            return writer_lock(std::unique_lock(p1.tableRwMutex));
        }

        // These functions are not thread-safe.
        char* add_atom_unsafe(const char* string, size_t stringSize);

        string_handle add_concat_unsafe(size_t hash, string_handle left, string_handle right);

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
        InternResult do_concat_unsafe(
            size_t hash,
            string_handle left,
            string_handle right,
            bool haveWriterLock,
            string_handle& result);

        // Statistics.
        size_t totalInternRequestSize = 0;
        size_t totalInternRequestCount = 0;
        size_t internHits = 0;
        size_t internMisses = 0;

    public:
        /**
         * Creates a new stringpool.
         * @param initial_table_capacity Initial capacity of the internal table, which has one entry for each interned string.
         */
        pool(size_t initial_table_capacity);

        /**
         * Creates a new stringpool.
         */
        pool();

        ~pool();

        /**
         * Gets a cached version of the given string, adding it to the cache if not already present.
         * @param string The string to cache.
         * @return Handle to the cached version of the string.
         */
        string_handle intern(const char* string);

        /**
         * Gets a cached version of the given string, adding it to the cache if not already present.
         * @param string The string to cache.
         * @param size The length of the string to cache.
         * @return Handle to the cached version of the string.
         */
        string_handle intern(const char* string, size_t size);

        /**
         * Gets a cached version of the given string, adding it to the cache if not already present.
         * @param string The string to cache.
         * @return Handle to the cached version of the string.
         */
        string_handle intern(std::string_view string);

        /**
         * Gets a cached version of the string represented by the concatenation of two given ones,
         * adding it to the cache if not already present.
         * @param left The first string forming the concatenation.
         * @param right The second string forming the concatenation.
         * @return Handle to the cached version of the concatenation.
         */
        string_handle concat(string_handle left, string_handle right);

        /**
         * Gets the overall number of bytes in strings passed to intern calls.
         */
        [[nodiscard]] size_t get_total_intern_request_size() const;

        /**
         * Gets the overall number of calls made to intern.
         */
        [[nodiscard]] size_t get_total_intern_request_count() const;

        /**
         * Gets the number calls made to intern that returned without adding anything to the cache.
         */
        [[nodiscard]] size_t get_total_intern_request_hits() const;

        /**
         * Gets the number of calls made to intern that resulted in data being added to the cache.
         */
        [[nodiscard]] size_t get_total_intern_request_misses() const;

        /**
         * Gets the approximate size of the data cache of this instance.
         */
        [[nodiscard]] size_t get_data_size() const;
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

#pragma once
#include <atomic>
#include <cstddef>
#include <deque>
#include <list>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <iterator>

namespace stringpool
{
    namespace internal
    {
        class weak_string_handle;
    }

    struct allocator
    {
        virtual ~allocator() = default;

        virtual char* allocate(size_t size) = 0;

        virtual void deallocate(char* ptr, size_t size) = 0;
    };

    class pool;

    class string_handle
    {
        friend class pool;
        friend class internal::weak_string_handle;
        const char* data;

#ifdef STRINGPOOL_TRACK_OWNERS
        pool* owner;

        string_handle(const char* data, pool* owner);
#else
        string_handle(const char* data);
#endif

        string_handle() = default;

        class tree_walker
        {
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

        class reverse_tree_walker
        {
            const char* root;
            std::deque<const char*> toVisit;

        public:
            reverse_tree_walker();

            reverse_tree_walker(const char* root);

            [[nodiscard]] size_t get_next_bytes(const char** bytes);

            [[nodiscard]] bool operator==(const reverse_tree_walker&) const = default;
        };

        [[nodiscard]] static bool concat_equals(const char* single, const char* left, const char* right);

        [[nodiscard]] static bool equals(const char* leftNode, const char* rightString, size_t length);

        [[nodiscard]] static int memcmp(const char* leftNode, const char* rhs, size_t length);

        class char_iterator_forward
        {
            tree_walker walker;
            const char* chunk;
            size_t chunkSize;
            size_t indexInChunk;

        public:
            using value_type = char;
            using difference_type = std::ptrdiff_t;

            char_iterator_forward();

            explicit char_iterator_forward(const string_handle& sh);

            char_iterator_forward(const char_iterator_forward&) = default;

            char_iterator_forward& operator=(const char_iterator_forward&) = default;

            [[nodiscard]] value_type operator*() const;

            char_iterator_forward& operator++();

            char_iterator_forward operator++(int);

            [[nodiscard]] bool operator==(const char_iterator_forward& other) const;

            [[nodiscard]] bool operator!=(const char_iterator_forward& other) const;
        };

        static_assert(std::forward_iterator<char_iterator_forward>);

        class char_iterator_backward
        {
            reverse_tree_walker walker;
            const char* chunk;
            size_t chunkSize;
            size_t indexInChunk;

        public:
            using value_type = char;
            using difference_type = std::ptrdiff_t;

            char_iterator_backward();

            explicit char_iterator_backward(const string_handle& sh);

            char_iterator_backward(const char_iterator_backward&) = default;

            char_iterator_backward& operator=(const char_iterator_backward&) = default;

            [[nodiscard]] value_type operator*() const;

            char_iterator_backward& operator++();

            char_iterator_backward operator++(int);

            [[nodiscard]] bool operator==(const char_iterator_backward& other) const;

            [[nodiscard]] bool operator!=(const char_iterator_backward& other) const;
        };

        static_assert(std::forward_iterator<char_iterator_backward>);

        void refcount_decrement();

        static void refcount_inc(char* data);
        void refcount_increment();

        static void refcount_dec(char* data, pool& owner);
        static void refcount_dec_unsafe(char* data, pool& owner);

        // Returns true if reference count reached zero.
        static bool refcount_dec_prefix(char* data);

        static void actually_delete_unsafe(char* data, pool& owner, size_t hash);

        static void maybe_decrement_children_refcounts(char* data, pool& owner);

        static void visit_chunks(const char* node,
                                 void (*callback)(const char* piece, size_t pieceSize, void* state),
                                 void* state);

        static size_t copy(const char* data, char* destination, size_t destination_size);

    public:
        string_handle(string_handle& other);

        string_handle(const string_handle& other);

        string_handle(string_handle&& other) noexcept;

        string_handle& operator=(const string_handle& other) noexcept;

        string_handle& operator=(string_handle&& other) noexcept;

        ~string_handle();

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
         * Gets a forward iterator pointing to the first character in this string.
         */
        [[nodiscard]] char_iterator_forward begin() const;

        /**
         * Gets a forward iterator pointing one character past the last character in this string.
         */
        [[nodiscard]] char_iterator_forward end() const;

        /**
         * Gets a backward iterator pointing to the last character in this string.
         */
        [[nodiscard]] char_iterator_backward rbegin() const;

        /**
         * Gets a backward iterator pointing one character before the first character in this string.
         */
        [[nodiscard]] char_iterator_backward rend() const;
    };

    namespace internal
    {
        class weak_string_handle
        {
            friend class stringpool::pool;
            friend class stringpool::string_handle;
            const char* data;

#ifdef STRINGPOOL_TRACK_OWNERS
            stringpool::pool* owner;

            weak_string_handle(const char* data, pool* owner);
#else
            weak_string_handle(const char* data);
#endif

            [[nodiscard]] stringpool::string_handle make_strong() const;

            weak_string_handle() = default;
        };
    }

    class pool
    {
        std::vector<char*> data;
        size_t totalDataSize = 0;

        [[nodiscard]] char* add_buffer(size_t size);

        void free_buffer(char* node);

        friend class string_handle;

        // Prevents concurrent changes to table contents.
        std::shared_mutex tableRwMutex;

        // key: hash
        // value: list of strings having that hash
        std::unordered_map<size_t, std::list<internal::weak_string_handle>> table;

        // These functions are not thread-safe.
        char* add_atom_unsafe(const char* string, size_t stringSize, size_t hash);

        internal::weak_string_handle add_concat_unsafe(size_t hash, string_handle left, string_handle right);

        enum class InternResult
        {
            Success = 0,
            NeedWriterLock = 1
        };

        // Caller must hold reader (or writer) lock.
        InternResult do_intern_unsafe(size_t hash,
                                      const char* string,
                                      size_t size,
                                      bool haveWriterLock,
                                      internal::weak_string_handle& result);

        // Caller must hold reader (or writer) lock on this and left and right owners.
        InternResult do_concat_unsafe(
            size_t hash,
            string_handle left,
            string_handle right,
            bool haveWriterLock,
            internal::weak_string_handle& result);

        // Statistics.
        size_t totalInternRequestSize = 0;
        size_t totalInternRequestCount = 0;
        size_t internHits = 0;
        size_t internMisses = 0;

        allocator* alloc;

    public:
        /**
         * Creates a new stringpool.
         */
        pool();

        /**
         * Creates a new stringpool.
         * @param initial_table_capacity Initial capacity of the internal table, which has one entry for each interned string.
         */
        pool(size_t initial_table_capacity);

        /**
         * Creates a new stringpool.
         * @param initial_table_capacity Initial capacity of the internal table, which has one entry for each interned string.
         * @param allocator An allocator that will be used for string data.
         * Some other metadata will still use the default heap allocator.
         */
        pool(size_t initial_table_capacity, stringpool::allocator* allocator);

        /**
         * Creates a new stringpool.
         * @param allocator An allocator that will be used for string data.
         * Some other metadata will still use the default heap allocator.
         */
        pool(stringpool::allocator* allocator);

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

template <>
struct std::hash<stringpool::string_handle>
{
    std::size_t operator()(stringpool::string_handle const& s) const noexcept
    {
        return s.hash();
    }
};

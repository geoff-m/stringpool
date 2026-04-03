#pragma once
#include <cstddef>

namespace stringpool {
    struct allocator {
        char* allocate(size_t size);

        void deallocate(char* ptr, size_t size);

        char* reallocate(char* ptr, size_t oldSize, size_t newSize);
    };

    class pool;

    class string_handle {
        friend class pool;
        pool& owner;
        char* data;

        string_handle(pool& owner, char* data);

        class tree_walker {
            char* root;
            char* lastLeaf;

        public:
            explicit tree_walker(char* root);

            [[nodiscard]] size_t get_next_bytes(char** bytes);
        };

    public:
        size_t copy(char* destination, size_t size) const;

        /**
         * Compares this string with the given one.
         * @param rhs The null-terminated string to compare to this one.
         * @return The sign of the difference of the first byte that differs, or zero if none differ.
         */
        [[nodiscard]] int strcmp(const char* rhs) const;

        /**
         * Compares this string with the given one.
         * Same as string_handle::memcmp(const string_handle&).
         * @param rhs The string to compare to this one.
         * @return The sign of the difference of the first byte that differs, or zero if none differ.
         */
        [[nodiscard]] int strcmp(const string_handle& rhs) const;

        /**
         * Compares this string with the given one.
         * Same as string_handle::strcmp(const string_handle&).
         * @param rhs The string to compare to this one.
         * @return The sign of the difference of the first byte that differs, or zero if none differ.
         */
        [[nodiscard]] int memcmp(const string_handle& rhs) const;

        /**
         * Compares this string with the given one.
         * @param rhs The string to compare to this one.
         * @return The sign of the difference of the first byte that differs, or zero if none differ.
         */
        [[nodiscard]] int memcmp(const char* rhs, size_t rhsLength) const;

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
        [[nodiscard]] bool equals(const string_handle& rhs) const;
    };

    class pool {
        char* data;
        size_t dataSize = 0;
        size_t dataCapacity;
        char** table;
        size_t tableCapacity;
        size_t tableSize = 0;
        size_t tableSizeBeforeReallocate = 0;

        enum class TableState {
            Ready,
            Busy
        };

        TableState tableState = TableState::Ready;

        class table_lock {
            pool& owner;

        public:
            table_lock(pool& owner);

            ~table_lock();
        };

        void updateTableSize(size_t newSize);

        void updateDataSize(size_t newSize);

        // These functions are not thread-safe.
        char* addAtom(const char* string, size_t size, const char* parent);

        [[nodiscard]] static bool equals(const char* string, size_t size, const char* entry);

    public:
        pool(size_t initial_table_capacity, size_t initial_data_capacity);

        pool();

        ~pool();

        string_handle intern(const char* string);

        string_handle intern(const char* string, size_t size);

        string_handle intern_concat(const char* left, const char* right);

        string_handle intern_concat(const char* left, size_t leftSize, const char* right, size_t rightSize);
    };
}

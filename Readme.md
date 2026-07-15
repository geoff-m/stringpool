# stringpool
A string interning library with concatenation.

## Overview
This library consists of two types: `stringpool` and `string_handle`.

Instances of `stringpool` offer just one main function, `intern`,
whose usage is simple.
`intern` takes a string argument and returns a `string_handle`
representing a cached version of the given string,
inserting it into the pool's cache if not already present.
Once added to the pool, strings live there until their reference count drops to zero.
If you disable reference counting (pass `-DSTRINGPOOL_REFCOUNT_ENABLE=OFF` to CMake),
strings will instead persist until the pool is destroyed.

You can then store and these instances of `string_handle` in place of your normal strings.
In this way, you can achieve deduplication by sacrificing a bit of convenience.

## Requirements
C++20 or later.
Depends on:
 - [xxhash](https://xxhash.com/)
 - [gtest](https://github.com/google/googletest) (only for testing)

stringpool can use vcpkg to acquire its dependencies automatically.
See [INSTALL.md](INSTALL.md).

## Usage
### string_handle
A `string_handle` is a small object that refers to a string that lives in a `pool`.
It is like a `const std::string` or `const char*` for most intents and purposes,
with the caveat that there is no API for viewing it as a single contiguous buffer.
Nevertheless, it has a rich set of accessors:
 - `size()`/`length()`
 - `copy(char* destination, size_t length)`
 - `to_string()` - create a `std::string` copy
 - `hash()` - get a non-cryptographic hash
 - `begin()`/`end()` - forward char iterators
 - `rbegin()`/`rend()` - backward char iterators
 - `strcmp(const char* rhs)`
 - `strcmp(const string_handle& rhs)`
 - `memcmp(const string_handle& rhs, size_t length)` 
 - `memcmp(const char* rhs, size_t length)`
 - `equals(const char* rhs, size_t length)`
 - `equals(std::string_view rhs)`
 - `equals(const char* rhs)`
 - `equals(const string_handle& rhs)`
 - `visit_chunks(void (*callback)(const char* chunk, size_t chunk_size, void* state), void* state)`
 - `visit_chunks(void (*callback)(std::string_view chunk, void* state), void* state)`
 - `begin_chunk()`/`end_chunk()` - chunkwise forward iterator
 - `rbegin_chunk()`/`rend_chunk()` - chunkwise backward iterator
 - `operator==`, `operator<`

Specializations of `std::hash` and of `std::formatter` are also defined.

### Concatenation
In addition to `intern`, there is also `concat`,
a function which takes two `string_handle` arguments
and returns a `string_handle` representing the concatenation of the two.
`concat` uses only O(1) memory.
Both `string_handle` arguments to `concat` must belong to the same pool.

In addition to being a useful operation per se,
the `concat` opeartion also makes `intern` more general.
This is because the pool deduplicates strings regardless of their concatenation structure.
For example, each of the following lines will produce a `string_handle`
identical to all the other lines:
```c++
auto path1 = p.intern("/foo/bar/baz");
auto path2 = p.concat(p.intern("/foo"), p.intern("/bar/baz"));
auto path3 = p.concat(p.intern("/foo/bar"), p.intern("/baz"));
auto path4 = p.concat(p.intern("/foo"), p.concat(p.intern("/bar"), p.intern("/baz")));
auto path5 = p.concat(p.concat(p.intern("/foo"), p.intern("/bar")), p.intern("/baz"));
```

### Efficiently accessing interned strings
Although char iterator functions are provided
for convenience, these access the string only a char at a time
and should be avoided whenever speed is a concern.
Meanwhile, `copy` and `to_string` can be used to copy the interned string
to a user-provided location from where it can be efficiently accessed,
but creating such a copy incurs a space and time cost.

Therefore, we offer `visit_chunks` and chunk iterators, APIs that
combine the speed advantage of processing the string in many-byte chunks
with the ability to avoid making a copy.

In the worst case, each of these chunks will be of size 1,
which makes these APIs no better than the char iterator approach.
But the opposite is more likely, i.e. that you will get a small number of chunks,
including the ideal case of just a single chunk containing the entire string.

#### visit_chunks
`visit_chunks` takes a `callback` and a `state` parameter.
It calls the callback possibly repeatedly,
each time passing to the callback a part of the string
represented by the `string_handle`.
The opaque `state` argument is also passed to the callback unmodified
and is not used by `visit_chunks` for any other purpose.
The sequence of chunks presented to the callback
represents a sequential partitioning of the string.

#### Chunk iterators
As an alternative to `visit_chunks`, you can use the iterators provided by
`begin_chunk` and `rbegin_chunk` to achieve the same effect.
Note that while `rbegin_chunk` presents the chunks in reverse order,
the char content within each chunk is not reversed.

#### Example using `write`
Consider the following example, in which we want to write an interned string to a file using POSIX `write`.
A function like the following will work but be inefficient, as it will always call `write`
once for each character.
```c++
void inefficient_write(int file, const string_handle& sh) {
    for (auto c : sh) {
        write(file, &c, 1);
    }
}
```
The implementation below is likely to be much more efficient by minimizing the number of `write` calls.
```c++
void write_chunk(const char* chunk, size_t chunk_size, void* file) {
    write(*static_cast<int*>(file), chunk, chunk_size);
}

void efficient_write(int file, const string_handle& sh) {
    sh.visit_chunks(write_chunk, &file);
}
```
`write(file, sh.to_string().c_str(), sh.size())` would also work
but would make a copy of the string, requiring additional time and space proportional to the string length.

Finally, the efficient version can also be written using `begin_chunk()`:
```c++
void efficient_write(int file, const string_handle& sh) {
    for (auto it = sh.begin_chunk(); it != sh.end_chunk(); ++it) {
        const auto chunk = *it;
        write(file, chunk.data(), chunk.size());
    }
}
```

### Memory management
#### Custom allocators
You can supply your own memory allocation functions to stringpool.
Inherit `stringpool::allocator` (whose declaration is shown below) and pass a pointer to an instance of this type to a `pool` constructor.
```c++
struct allocator {
    virtual ~allocator() = default;
    virtual char* allocate(size_t size) {
        return allocate(size, 1);
    }
    virtual char* allocate(size_t size, size_t alignment) = 0;
    virtual void deallocate(char* ptr, size_t size) = 0;
};
```
stringpool will use the custom allocator for all interned string content.
However, even when a custom allocator is supplied, stringpool will still use the default allocator for some bookkeeping.

#### Statistics
`pool` provides some statistics functions that can be used to get a notion of how much the pool is costing and saving you.
The declarations of these functions are reproduced below, but see the header comments for their documentation.
```c++
size_t get_total_intern_request_size() const;
size_t get_total_intern_request_count() const;
size_t get_total_intern_request_hits() const;
size_t get_total_intern_request_misses() const;
size_t get_data_size() const;
```
For example, given a pool `p`, you could say that the quotient given by
`p.get_data_size() / (double)p.get_total_intern_request_size()`
is the "compression ratio" realized in `p`, with lower values meaning more space savings.

Adding a string to a pool will allocate a O(1) number of extra bytes in addition to the size of the string.
Beyond this, a small amount of additional bookkeeping data,
linear in the number of interned strings,
is also allocated on the default heap but not measured by the above statistics functions.

## Use cases
### Application types
Consider string interning when your application handles
a large number of strings (or a small number of large strings)
among which many strings are not unique.
This includes software like
 - HTTP servers
 - compilers
 - file transfer and archiving tools

and any other program that's likely to have identical strings
sitting in memory, such as directory names, URL elements,
or identifiers parsed from text files.

### Performance considerations
The motivation for string interning is to trade time for space.
You gain significant memory savings, provided you have large and/or numerous duplicated strings.
You pay the time needed to maintain the pool.
The time cost of using the pool is linear in the length of your strings.
This makes it easy to justify adding string interning to a system,
because the system almost certainly already pays a linear time cost (or more) to do whatever it does with strings.

Although it may be wise to expect adopting string interning
to slow down your application, it is not guaranteed.
In fact, some applications may be *sped up* if they make frequent equality comparisons between strings.
Since a stringpool::pool deduplicates strings, equality comparisons on pairs of string handles
reduce to simply comparing pointers, a fast constant-time operation.

Another potential speed improvement comes from the fact that the string pool
can help your application avoid allocating memory.
Your allocator (up to and including the operating system) will have less work to do
the less memory pressure there is.

This library aims to be efficiently thread-safe.
Most operations run efficiently in parallel,
but making all threads share a mutable pool inevitably requires synchronization.
Adding/deleting strings to/from a pool are the most likely to
cause contention, whereas applications that mostly operate on existing string handles
should scale well with thread count.


This library is not NUMA-aware.

## Further reading
Some implementation details are summarized [on my blog](https://geoff.space/2026/04/stringpool-internals/).

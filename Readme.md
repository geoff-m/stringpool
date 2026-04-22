# stringpool
A string interning library with concatenation.

## Usage
The string pool offers just one main function, `intern`,
whose usage is simple.
`intern` takes a string argument and returns a `string_handle`
representing a cached version of the given string,
inserting it into the pool's cache if not already present.
Once added to the pool, strings live there until their reference count drops to zero.
If you disable reference counting (pass `-DSTRINGPOOL_REFCOUNT_ENABLE=OFF` to CMake),
strings will instead persist until the pool is destroyed.

You can then store these handles in place of your normal strings.
In this way, you can achieve deduplication by sacrificing a bit of convenience.

### string_handle
A `string_handle` is a small object that refers to a string that lives in a `pool`.
It is like a `const std::string` or `const char*` for most intents and purposes,
with the caveat that there is no API for viewing it as a single contiguous buffer.
Nevertheless, it has a rich set of accessors:
 - `size()`/`length()`
 - `copy(char* destination, size_t length)`
 - `to_string()` - create a `std::string` copy
 - `hash()` - get a non-cryptographic hash
 - `begin()`/`end()` - char iterators
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

### Efficiently accessing interned strings with `visit_chunks`
Although iterator functions are provided
for convenience, these access the string only a char at a time
and should be avoided whenever speed is a concern.
Meanwhile, `copy` and `to_string` can be used to copy the interned string
to a user-provided location from where it can be efficiently accessed,
but creating such a copy incurs a space and time cost.

Therefore, we offer `visit_chunks`, an API that
combines the speed advantage of processing the string in many-byte chunks
with the ability to avoid making a copy.
`visit_chunks` takes a `callback` and a `state` parameter.
It calls the callback possibly repeatedly,
each time passing to the callback a part of the string
represented by the `string_handle`.
The opaque `state` argument is also passed to the callback unmodified
and is not used by `visit_chunks` for any other purpose.
The sequence of chunks presented to the callback
represents a sequential partitioning of the string.

In the worst case, each of these chunks will be of size 1,
which makes `visit_chunks` no better than the char iterator approach.
But the opposite is more likely, i.e. that you will get a small number of chunks,
including the ideal case of just a single chunk containing the entire string.

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
but would make a copy of the string, thereby defeating the purpose of interning.

### Concatenation
In addition to `intern`, there is also `concat`,
a function which takes two `string_handle` arguments
and returns a `string_handle` representing the concatenation of the two.
`concat` uses only O(1) memory.
Both `string_handle` arguments to `concat` must belong to the same `pool`. 

## Use cases
Consider string interning when your application handles
a large number of strings (or a small number of large strings)
among which most strings are not unique.
This includes software like
 - HTTP servers
 - compilers
 - file transfer and archiving tools

and any other program that's likely to have identical strings
sitting in memory, such as file names, URL elements,
or identifiers parsed from text files.

Although string interning's main goal of reducing memory usage
typically comes at a modest time cost, it is not guaranteed that your application will be slowed down.
In fact, some applications may be *sped up* if they make frequent equality comparisons between strings.
Since a stringpool::pool deduplicates strings, equality comparisons on pairs of string handles
reduce to simply comparing pointers, a fast constant-time operation.

The inclusion of the `concat` function makes `stringpool` slightly more general,
not merely because concatenation is a useful operation,
but also because the pool deduplicates strings regardless of their concatenation structure.
For example, each of the following lines will produce a `string_handle`
identical to all the other lines:
```c++
auto path1 = p.intern("/foo/bar/baz");
auto path2 = p.concat(p.intern("/foo"), p.intern("/bar/baz"));
auto path3 = p.concat(p.intern("/foo/bar"), p.intern("/baz"));
auto path4 = p.concat(p.intern("/foo"), p.concat(p.intern("/bar"), p.intern("/baz")));
auto path5 = p.concat(p.concat(p.intern("/foo"), p.intern("/bar")), p.intern("/baz"));
```

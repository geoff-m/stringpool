# stringpool
A string interning library with concatenation.

## Usage
The string pool offers just one main function, `intern`,
whose usage is simple.
`intern` performs the following:
1. Checks whether your string is already stored in the pool.
If it is, `intern` returns you a handle to that string. 
3. Otherwise, `intern` copies your string into the pool,
and then returns you a handle to it.

You can then store these handles, of type `string_handle`,
in place of your normal string types.
In this way, you can achieve deduplication by sacrificing a bit of convenience.

### string_handle
A `string_handle` is a pointer-sized object that refers to a
string that lives in a `stringpool`.
It is like a `const std::string` or `const char*` for most intents and purposes,
with the caveat that there is no API for viewing it as a single contiguous buffer.
Nevertheless, it has a rich set of accessors:
 - `size()`/`length()`
 - `copy(char* destination, size_t length)`
 - `to_string()` - create a std::string copy
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

### Efficiently accessing interned strings
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
represents a partitioning of the string.

In the worst case, each of these chunks will be of size 1,
which makes `visit_chunks` no better than the char iterator approach.
But the opposite is more likely, i.e. that you will get a small number of chunks,
including the ideal case of just a single chunk containing the entire string.

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

The inclusion of the `concat` function makes `stringpool` slightly more general,
not merely because concatenation is a useful operation,
but also because the pool deduplicates strings regardless of their concatenation structure.
For example, each of the following lines will produce a `string_handle`
identical to all the other lines:
```c++
auto path1 = p.intern("/foo/bar/baz");
auto path2 = p.concat(p.intern("/foo"), p.intern("/bar/baz"));
auto path2 = p.concat(p.intern("/foo/bar"), p.intern("/baz"));
auto path3 = p.concat(p.intern("/foo"), p.concat(p.intern("/bar"), p.intern("/baz")));
auto path4 = p.concat(p.concat(p.intern("/foo"), p.intern("/bar")), p.intern("/baz")));
```

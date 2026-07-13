# Building with stringpool
stringpool is essentially just a CMake project. It depends on C++20, [gtest](https://github.com/google/googletest), and [xxhash](https://xxhash.com/). If CMake can find these on your system, stringpool should build. However, you will likely find using vcpkg to be easier.

## Setting up a vcpkg dependency on stringpool
When your project uses vcpkg to depend on stringpool, vcpkg will automatically acquire stringpool's transitive dependencies.
If you're new to vcpkg, consult [vcpkg documentation](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started), as the steps here don't cover setup of vcpkg itself.

1. Add `"stringpool"` to your list of dependencies in your project's `vcpkg.json`.

2. If you don't already have a file named `vcpkg-configuration.json` in your project, add one with the following content. This file should be located in the same directory as your `vcpkg.json`.
```json
{
  "default-registry": {
    "kind": "builtin",
    "baseline": "<latest git commit hash of official registry>"
  },
  "registries": [
    {
      "kind": "git",
      "repository": "https://github.com/geoff-m/vcpkg-registry",
      "baseline": "<latest git commit hash of geoff-m/vcpkg-registry>",
      "packages": ["stringpool"]
    }
  ]
}
```
(If your project already has a `vcpkg-configuration.json`, merge the `"registries"` entry above into it.)

3. Do a CMake + vcpkg build as you normally would (`cmake --preset ...`).

## CMake setup
1. Add a `find_package` declaration such as
```cmake
find_package(stringpool REQUIRED)
```
2. Link your target to `stringpool::stringpool`, e.g.
```cmake
target_link_libraries(my-app PRIVATE stringpool::stringpool)
```

Note that including stringpool requires at least C++20.
Linking to stringpool should automatically set `CMAKE_CXX_STANDARD` to 20 if it is unspecified.
If you force the language version to less than 20, expect the build to fail. 

## Include stringpool.h

Sample usage:
```c++
#include <stringpool.h>

int main() {
    stringpool::pool p;
    
    return 0;
}
```

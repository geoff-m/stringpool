
get_property(isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(isMultiConfig)
    if(NOT "Tsan" IN_LIST CMAKE_CONFIGURATION_TYPES)
        list(APPEND CMAKE_CONFIGURATION_TYPES Tsan)
    endif()
endif()

set(CMAKE_C_FLAGS_ASAN
        "${CMAKE_C_FLAGS_DEBUG} -fsanitize=thread" CACHE STRING
        "Flags used by the C compiler for Tsan build type or configuration." FORCE)

set(CMAKE_CXX_FLAGS_ASAN
        "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=thread" CACHE STRING
        "Flags used by the C++ compiler for Tsan build type or configuration." FORCE)

set(CMAKE_EXE_LINKER_FLAGS_ASAN
        "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=thread" CACHE STRING
        "Linker flags to be used to create executables for Tsan build type." FORCE)

set(CMAKE_SHARED_LINKER_FLAGS_ASAN
        "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -fsanitize=thread" CACHE STRING
        "Linker flags to be used to create shared libraries for Tsan build type." FORCE)
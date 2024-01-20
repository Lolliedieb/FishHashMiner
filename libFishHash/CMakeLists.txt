if (MSVC)
set(CMAKE_CXX_STANDARD 20)
endif()

if (UNIX)
set(CMAKE_CXX_STANDARD 11)
endif()

add_library(
    FishHash
    FishHash.cpp
    3rdParty/blake3.c
    3rdParty/blake3_portable.c
    3rdParty/blake3_dispatch.c
    3rdParty/keccak.c
)




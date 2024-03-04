cmake_minimum_required(VERSION 3.1) 

project(fishHashMiner)
set(TARGET_NAME fishHashMiner)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(Boost_USE_MULTITHREADED ON)
#set(Boost_USE_STATIC_LIBS ON)
#set(Boost_USE_STATIC_RUNTIME ON)
#set(OPENSSL_USE_STATIC_LIBS TRUE)

if(UNIX)
    find_library(DL_LIB libdl.a REQUIRED)
endif()

find_package(Boost REQUIRED COMPONENTS system)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
  link_directories(${Boost_LIBRARY_DIRS})
endif()

if (MSVC)
    # use static runtime
    foreach(flag_var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO
        flag_var CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)

        if(${flag_var} MATCHES "/MD")
            string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        endif(${flag_var} MATCHES "/MD")
    endforeach(flag_var)

    set(OPENSSL_USE_STATIC_LIBS TRUE)
    set(OPENSSL_MSVC_STATIC_RT TRUE)
endif()

find_package(OpenSSL REQUIRED)
if( OpenSSL_FOUND )   
    link_directories(${OPENSSL_LIBRARIES})
    include_directories(${OPENSSL_INCLUDE_DIR})
    message(STATUS "Using OpenSSL ${OPENSSL_VERSION}")
else()
    message(FATAL_ERROR "error, OpenSSL not found, please install: 'apt-get install libssl-dev' on Ubuntu")
endif()

#find_package(OpenCLHeaders REQUIRED)
#find_package(OpenCLICDLoader REQUIRED)
#find_package(OpenCLHeadersCpp REQUIRED)

find_package(OpenCL REQUIRED)

# Miner Sources & Headers
set (HEADERS
        
)

set(MINER_SRC
    src/clHost.cpp
    src/ironFishStratum.cpp				
    src/main.cpp
)

add_executable(${TARGET_NAME} ${MINER_SRC} ${HEADERS}  )

target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/libFishHash)

add_subdirectory(libFishHash)

target_compile_options(${TARGET_NAME} PRIVATE -Werror -Wall -Wextra)


# Linking packages
#target_link_libraries(${TARGET_NAME} OpenCL::Headers OpenCL::OpenCL OpenCL::HeadersCpp)
target_link_libraries(${TARGET_NAME} OpenCL::OpenCL)
target_link_libraries(${TARGET_NAME} FishHash)

if(MSVC)
    target_link_libraries(${TARGET_NAME} crypt32.lib)
    if(MSVC_VERSION GREATER_EQUAL 1900)
        target_link_libraries(${TARGET_NAME} legacy_stdio_definitions.lib)
    endif()
endif()

if(UNIX) 
	target_link_libraries(${TARGET_NAME}  -ldl)
endif()

target_link_libraries(${TARGET_NAME} ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY} ${Boost_LIBRARIES} OpenCL::OpenCL)

if(UNIX)
    find_package(Threads)
    target_link_libraries(${TARGET_NAME} Threads::Threads)
endif()




cmake_minimum_required(VERSION 3.1.2)
project(WSPC CXX)

# Require C++14 standard without GNU extensions
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
                 "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

if(MSVC)
    set(Boost_USE_STATIC_LIBS ON)
endif()
find_package(Boost 1.58.0 REQUIRED system date_time regex)

if(MSVC)
    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
        add_compile_options("/W4")
    endif()

    add_definitions(-D_SCL_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS)

    add_compile_options(/wd4996) # Deprecated stuff
    add_compile_options(/wd4127) # conditional expression is constant
    add_compile_options(/wd4100) # unreferenced formal parameter

    if(CMAKE_CL_64)
        # Visual Studio bails out on debug builds in 64bit mode unless this flag is set...
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /bigobj")
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /bigobj")
    endif()
elseif((CMAKE_CXX_COMPILER_ID STREQUAL "Clang") OR
       (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))

    # Turn on standard set of warnings
    add_compile_options(-Wall -pedantic -Wextra)
    add_compile_options(-Wno-unused-parameter)
endif()

add_subdirectory(external/kl)

set(WSPC_SOURCE_FILES
    src/wspc/service_handler.cpp
    src/wspc/service.cpp
    src/wspc/transport.cpp
    src/wspc/type_description.cpp
    src/wspc/typed_service_handler.cpp)
set(WSPC_HEADER_FILES
    src/wspc/service_handler.hpp
    src/wspc/service.hpp
    src/wspc/transport.hpp
    src/wspc/type_description.hpp
    src/wspc/typed_service_handler.hpp)

add_library(wspc STATIC
    ${WSPC_SOURCE_FILES}
    ${WSPC_HEADER_FILES})
target_link_libraries(wspc PUBLIC ${Boost_LIBRARIES})
target_link_libraries(wspc PUBLIC kl)
target_include_directories(wspc PUBLIC src)
target_include_directories(wspc PRIVATE external/websocketpp)
target_include_directories(wspc SYSTEM PRIVATE ${Boost_INCLUDE_DIRS})
if(UNIX)
    target_link_libraries(wspc PRIVATE pthread)
endif()

add_executable(example example/server.cpp)
target_link_libraries(example wspc)
target_include_directories(example SYSTEM PRIVATE ${Boost_INCLUDE_DIRS})

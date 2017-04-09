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
find_package(Boost 1.58.0 REQUIRED COMPONENTS
    system date_time regex)

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
target_include_directories(wspc 
    PUBLIC src
    PRIVATE external/websocketpp)
target_link_libraries(wspc 
    PRIVATE Boost::disable_autolinking
    PRIVATE Boost::system
    PRIVATE Boost::date_time
    PRIVATE Boost::regex
    PUBLIC kl)

if(UNIX)
    target_link_libraries(wspc PRIVATE pthread)
endif()

add_executable(example example/server.cpp)
target_link_libraries(example 
    PUBLIC wspc Boost::boost 
    PRIVATE Boost::disable_autolinking)

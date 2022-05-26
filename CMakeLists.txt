cmake_minimum_required(VERSION 3.20)

set(CMAKE_DISBLAE_IN_SOURCE_BUILD ON)
project(anycast)


list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CMakeFlags)
include(CompilerFlags)
include(Win)

find_package(Threads)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)
set(Boost_NO_BOOST_CMAKE ON)
set(Boost_NO_WARN_NEW_VERSIONS ON)
find_package(Boost 1.78 REQUIRED COMPONENTS program_options coroutine filesystem context date_time json nowide)

add_subdirectory(deps/openssl)
add_subdirectory(deps/fmt)

add_subdirectory(src)
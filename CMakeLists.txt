cmake_minimum_required(VERSION 3.12)

project(lms)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake/modules/)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
if (UNIX AND NOT APPLE)
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
endif ()

option(ENABLE_TESTS "Enable tests" ON)

if(ENABLE_TESTS)
	include(CTest)
	find_package(GTest REQUIRED)
endif()

# Common dependencies
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(Filesystem REQUIRED)
find_package(Boost REQUIRED COMPONENTS system program_options iostreams)
find_package(Wt REQUIRED COMPONENTS Wt Dbo DboSqlite3 HTTP)

# WT
if (NOT Wt_FOUND)
	message(FATAL_ERROR "Wt package not found!")
endif ()
if (NOT TARGET Wt::Wt)
	message(FATAL_ERROR "Cannot find Wt::Wt!")
endif ()
if (NOT TARGET Wt::Dbo)
	message(FATAL_ERROR "Cannot find Wt::Dbo!")
endif ()
if (NOT TARGET Wt::DboSqlite3)
	message(FATAL_ERROR "Cannot find Wt::DboSqlite3!")
endif ()
if (NOT TARGET Wt::HTTP)
	message(FATAL_ERROR "Cannot find Wt::HTTP!")
endif ()

# Benchmark
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
if (BUILD_BENCHMARKS)
	find_package(benchmark REQUIRED)
	message(STATUS "Building benchmarks")
endif()

if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DNDEBUG)
endif()

add_subdirectory(src)

install(DIRECTORY approot DESTINATION share/lms)
install(DIRECTORY docroot DESTINATION share/lms)
install(FILES conf/systemd/default.service DESTINATION share/lms)
install(FILES conf/pam/lms DESTINATION share/lms)
install(FILES conf/lms.conf DESTINATION share/lms)


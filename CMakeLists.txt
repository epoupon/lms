cmake_minimum_required(VERSION 3.12)

project(lms)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(CTest)
find_package(PkgConfig REQUIRED)
find_package(Filesystem REQUIRED)
find_package(FFMPEGAV REQUIRED)
find_package(Taglib REQUIRED)
find_package(Boost REQUIRED COMPONENTS system program_options)
find_package(PStreams REQUIRED)
find_package(PAM)
find_package(STB)
pkg_check_modules(GRAPHICSMAGICKXX GraphicsMagick++)

if (STB_FOUND)
	set(IMAGE_LIBRARY STB CACHE STRING "STB library")
elseif (GRAPHICSMAGICKXX_FOUND)
	set(IMAGE_LIBRARY GraphicksMagick++ CACHE STRING "GraphicsMagick++")
else ()
	message(FATAL_ERROR "Cannot find suitable image library")
endif()
set_property(CACHE IMAGE_LIBRARY PROPERTY STRINGS GraphicksMagick++ STB)

if (IMAGE_LIBRARY STREQUAL GraphicksMagick++ AND NOT GRAPHICSMAGICKXX_FOUND)
	message(FATAL_ERROR "GraphicksMagick++ not found")
elseif (IMAGE_LIBRARY STREQUAL STB AND NOT STB_FOUND)
	message(FATAL_ERROR "STB not found")
endif ()

message(STATUS "IMAGE_LIBRARY set to ${IMAGE_LIBRARY}")

add_subdirectory(src)

install(DIRECTORY approot DESTINATION share/lms)
install(DIRECTORY docroot DESTINATION share/lms)
install(FILES conf/systemd/default.service DESTINATION share/lms)
install(FILES conf/pam/lms DESTINATION share/lms)
install(FILES conf/lms.conf DESTINATION share/lms)


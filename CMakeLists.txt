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
find_package(Boost REQUIRED COMPONENTS system)
find_package(PStreams REQUIRED)
pkg_check_modules(GRAPHICSMAGICKXX REQUIRED GraphicsMagick++)

add_subdirectory(src)

install(DIRECTORY approot DESTINATION share/lms)
install(DIRECTORY docroot DESTINATION share/lms)
install(FILES systemd/default.service DESTINATION share/lms)
install(FILES conf/lms.conf DESTINATION share/lms)


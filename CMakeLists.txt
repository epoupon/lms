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
find_package(PAM)
pkg_check_modules(GRAPHICSMAGICKXX REQUIRED GraphicsMagick++)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Werror -Wno-error=parentheses -Wno-error=unused-function -O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -g")

if(PAM_FOUND)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSEPAM")
endif(PAM_FOUND)

add_subdirectory(src)

# TODO pam configuration file
install(DIRECTORY approot DESTINATION share/lms)
install(DIRECTORY docroot DESTINATION share/lms)
install(FILES systemd/default.service DESTINATION share/lms)
install(FILES conf/lms.conf DESTINATION share/lms)


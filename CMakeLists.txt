cmake_minimum_required(VERSION 3.10)

project(lms)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(PkgConfig)
pkg_check_modules(IMAGEMAGICKXX REQUIRED ImageMagick++)

enable_testing()

add_subdirectory(src)

install(DIRECTORY approot DESTINATION share/lms)
install(DIRECTORY docroot DESTINATION share/lms)
install(FILES systemd/default.service DESTINATION share/lms)
install(FILES conf/lms.conf DESTINATION share/lms)


pkg_check_modules(Taglib REQUIRED IMPORTED_TARGET taglib)

if(BUILD_TESTING)
	add_subdirectory(test)
endif()

if (BUILD_BENCHMARKS)
	add_subdirectory(bench)
endif()

add_library(lmsmetadata SHARED
	impl/AvFormatTagReader.cpp
	impl/Lyrics.cpp
	impl/Parser.cpp
	impl/TagLibTagReader.cpp
	impl/Utils.cpp
	)

target_include_directories(lmsmetadata INTERFACE
	include
	)

target_include_directories(lmsmetadata PRIVATE
	include
	)

target_link_libraries(lmsmetadata PRIVATE
	lmsav
	PkgConfig::Taglib
	)

target_link_libraries(lmsmetadata PUBLIC
	lmscore
	std::filesystem
	)

install(TARGETS lmsmetadata DESTINATION ${CMAKE_INSTALL_LIBDIR})


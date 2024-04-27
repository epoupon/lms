pkg_check_modules(Config++ REQUIRED IMPORTED_TARGET libconfig++)
pkg_check_modules(Archive REQUIRED IMPORTED_TARGET libarchive)

add_library(lmscore SHARED
	impl/http/Client.cpp
	impl/http/SendQueue.cpp
	impl/ArchiveZipper.cpp
	impl/ChildProcess.cpp
	impl/ChildProcessManager.cpp
	impl/Config.cpp
	impl/FileResourceHandler.cpp
	impl/IOContextRunner.cpp
	impl/Logger.cpp
	impl/NetAddress.cpp
	impl/Path.cpp
	impl/Random.cpp
	impl/RecursiveSharedMutex.cpp
	impl/StreamLogger.cpp
	impl/String.cpp
	impl/TraceLogger.cpp
	impl/UUID.cpp
	impl/WtLogger.cpp
	)

target_include_directories(lmscore INTERFACE
	include
	)

target_include_directories(lmscore PRIVATE
	include
	)

target_link_libraries(lmscore PRIVATE
	PkgConfig::Config++
	PkgConfig::Archive
	)

target_link_libraries(lmscore PUBLIC
	Boost::system
	std::filesystem
	Wt::Wt
	)

install(TARGETS lmscore DESTINATION ${CMAKE_INSTALL_LIBDIR})

if(BUILD_TESTING)
	add_subdirectory(test)
endif()

if (BUILD_BENCHMARKS)
	add_subdirectory(bench)
endif()

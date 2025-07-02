pkg_check_modules(Config++ REQUIRED IMPORTED_TARGET libconfig++)
pkg_check_modules(Archive REQUIRED IMPORTED_TARGET libarchive)
pkg_check_modules(XXHASH REQUIRED IMPORTED_TARGET libxxhash)

add_library(lmscore STATIC
	impl/http/Client.cpp
	impl/http/SendQueue.cpp
	impl/ArchiveZipper.cpp
	impl/ChildProcess.cpp
	impl/ChildProcessManager.cpp
	impl/Config.cpp
	impl/FileResourceHandler.cpp
	impl/JobScheduler.cpp
	impl/IOContextRunner.cpp
	impl/Logger.cpp
	impl/MimeTypes.cpp
	impl/NetAddress.cpp
	impl/PartialDateTime.cpp
	impl/Path.cpp
	impl/Random.cpp
	impl/RecursiveSharedMutex.cpp
	impl/StreamLogger.cpp
	impl/String.cpp
	impl/TraceLogger.cpp
	impl/UUID.cpp
	impl/WtLogger.cpp
	impl/XxHash3.cpp
	)

target_include_directories(lmscore INTERFACE
	include
	)

target_include_directories(lmscore PRIVATE
	include
	${XXHASH_INCLUDE_DIRS}
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

if(BUILD_TESTING)
	add_subdirectory(test)
endif()

if (BUILD_BENCHMARKS)
	add_subdirectory(bench)
endif()

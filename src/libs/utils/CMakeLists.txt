add_library(lmsutils SHARED
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
	impl/UUID.cpp
	impl/WtLogger.cpp
	)

target_include_directories(lmsutils INTERFACE
	include
	)

target_include_directories(lmsutils PRIVATE
	include
	)

target_link_libraries(lmsutils PRIVATE
	PkgConfig::Config++
	PkgConfig::Archive
	)

target_link_libraries(lmsutils PUBLIC
	Boost::system
	std::filesystem
	Wt::Wt
	)

install(TARGETS lmsutils DESTINATION lib)

if(BUILD_TESTING)
	add_subdirectory(test)
endif()

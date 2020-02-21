
add_library(lmsutils SHARED
	impl/Config.cpp
	impl/ChildProcess.cpp
	impl/ChildProcessManager.cpp
	impl/Exception.cpp
	impl/Logger.cpp
	impl/NetAddress.cpp
	impl/Path.cpp
	impl/Random.cpp
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
	config++
	)

target_link_libraries(lmsutils PUBLIC
	boost_system
	stdc++fs
	wt
	)

install(TARGETS lmsutils DESTINATION lib)


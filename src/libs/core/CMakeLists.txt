pkg_check_modules(Archive REQUIRED IMPORTED_TARGET libarchive)
pkg_check_modules(Config++ REQUIRED IMPORTED_TARGET libconfig++)
pkg_check_modules(XXHASH REQUIRED IMPORTED_TARGET libxxhash)

include(CheckCXXCompilerFlag)

set(LMS_VERSION ${PROJECT_VERSION})

configure_file(
	impl/Version.cpp.in
	impl/Version.cpp
	@ONLY
	)

add_library(lmscore STATIC
	impl/http/Client.cpp
	impl/http/SendQueue.cpp
	impl/math/EuclideanDistance.cpp
	impl/math/StatsAccumulator.cpp
	impl/media/Codec.cpp
	impl/media/Container.cpp
	impl/media/ImageType.cpp
	impl/media/MimeType.cpp
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
	impl/String.cpp
	impl/TraceLogger.cpp
	impl/UUID.cpp
	impl/XxHash3.cpp
	${CMAKE_CURRENT_BINARY_DIR}/impl/Version.cpp
	)

check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
if (COMPILER_SUPPORTS_AVX2)
    target_sources(lmscore PRIVATE impl/math/EuclideanDistanceSIMD.cpp)
    set_source_files_properties(impl/math/EuclideanDistanceSIMD.cpp PROPERTIES 
		COMPILE_OPTIONS "-mavx2"
	    SKIP_UNITY_BUILD_INCLUSION ON
	)
    target_compile_definitions(lmscore PRIVATE LMS_SUPPORT_AVX2=1)
else()
    target_compile_definitions(lmscore PRIVATE LMS_SUPPORT_AVX2=0)
endif()

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
	OpenSSL::Crypto
	)

target_link_libraries(lmscore PUBLIC
	Wt::Wt
	)

if(BUILD_TESTING)
	add_subdirectory(test)
endif()

if (BUILD_BENCHMARKS)
	add_subdirectory(bench)
endif()

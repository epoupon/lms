
add_subdirectory(test)

add_library(lmsmetadata SHARED
	impl/AvFormatParser.cpp
	impl/TagLibParser.cpp
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
	lmsutils
	std::filesystem
	)

install(TARGETS lmsmetadata DESTINATION lib)


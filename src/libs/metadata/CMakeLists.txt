
add_library(lmsmetadata SHARED
	impl/AvFormatParser.cpp
	impl/TagLibParser.cpp
	)

target_include_directories(lmsmetadata INTERFACE
	include
	)

target_include_directories(lmsmetadata PRIVATE
	include
	)

target_link_libraries(lmsmetadata PRIVATE
	lmsav
	tag
	)

target_link_libraries(lmsmetadata PUBLIC
	lmsutils
	)

install(TARGETS lmsmetadata DESTINATION lib)


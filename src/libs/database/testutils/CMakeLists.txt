add_library(lmsdatabasetestutils STATIC
	TmpDatabase.cpp
	)

target_include_directories(lmsdatabasetestutils PUBLIC
	.
	)

target_include_directories(lmsdatabasetestutils PRIVATE
	../impl
	)

target_link_libraries(lmsdatabasetestutils PUBLIC
	lmsdatabase
	)

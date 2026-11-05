
add_executable(bench-database
	FilterExpr.cpp
	)

target_link_libraries(bench-database PRIVATE
	lmsdatabasetestutils
	benchmark
	)

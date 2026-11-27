
# Want as many meaningful warnings as possible
add_compile_options(-Wall -Wextra -pedantic)
# GCC has no -Wundef-prefix; plain -Wundef also fires on third-party headers (not an error for that reason)
add_compile_options($<IF:$<CXX_COMPILER_ID:Clang>,-Wundef-prefix=LMS_,-Wundef>)

add_subdirectory(libs)
add_subdirectory(lms)
add_subdirectory(tools)

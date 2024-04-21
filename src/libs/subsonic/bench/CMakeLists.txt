add_executable(bench-subsonic
       SubsonicBench.cpp
       )

target_include_directories(bench-subsonic PRIVATE
       ../impl
       )

target_link_libraries(bench-subsonic PRIVATE
       lmscore
       lmsdatabase
       lmssubsonic
       benchmark
       )
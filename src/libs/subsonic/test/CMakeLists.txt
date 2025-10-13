include(GoogleTest)

add_executable(test-subsonic
	ClientInfo.cpp
	Subsonic.cpp
	SubsonicResponse.cpp
	TranscodeDecision.cpp
	)

target_include_directories(test-subsonic PRIVATE
	../impl
	)

target_link_libraries(test-subsonic PRIVATE
	lmscore
	lmsaudio
	lmssubsonic
	GTest::GTest
	)

if (NOT CMAKE_CROSSCOMPILING)
	gtest_discover_tests(test-subsonic)
endif()


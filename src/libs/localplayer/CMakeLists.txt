
add_library(lmslocalplayer SHARED
	impl/LocalPlayer.cpp
	impl/pulseaudio/PulseAudioOutput.cpp
	)

target_include_directories(lmslocalplayer INTERFACE
	include
	)

target_include_directories(lmslocalplayer PRIVATE
	include
	)

target_link_libraries(lmslocalplayer PRIVATE
	lmsav
	lmsdatabase
	pulse
	)

install(TARGETS lmslocalplayer DESTINATION lib)


# Quick and dirty av* includes discoverer

find_path(AVCODEC_INCLUDE_DIR NAMES libavcodec/avcodec.h PATH_SUFFIXES ffmpeg)
find_library(AVCODEC_LIBRARY avcodec)

find_path(AVFORMAT_INCLUDE_DIR NAMES libavformat/avformat.h PATH_SUFFIXES ffmpeg)
find_library(AVFORMAT_LIBRARY avformat)

find_path(AVUTIL_INCLUDE_DIR NAMES libavutil/avutil.h PATH_SUFFIXES ffmpeg)
find_library(AVUTIL_LIBRARY avutil)

find_path(AVDEVICE_INCLUDE_DIR NAMES libavdevice/avdevice.h PATH_SUFFIXES ffmpeg)
find_library(AVDEVICE_LIBRARY avdevice)

include(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(
        FFMPEGAV
        FOUND_VAR FFMPEGAV_FOUND
        REQUIRED_VARS AVUTIL_LIBRARY AVFORMAT_LIBRARY
)

mark_as_advanced(AVFORMAT_LIBRARY)
mark_as_advanced(AVUTIL_LIBRARY)



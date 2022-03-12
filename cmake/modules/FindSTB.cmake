find_path(STB_INCLUDE_DIR stb/stb_image.h)

include(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(
		STB
		REQUIRED_VARS STB_INCLUDE_DIR
)



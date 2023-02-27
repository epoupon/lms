find_path(STB_INCLUDE_DIR stb_image.h PATH_SUFFIXES stb)

include(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(
		STB
		REQUIRED_VARS STB_INCLUDE_DIR
)



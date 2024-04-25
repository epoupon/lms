find_path(STB_IMAGE_INCLUDE_DIR stb_image.h PATH_SUFFIXES stb)

message("Current include paths:")
foreach(path ${CMAKE_INCLUDE_PATH})
    message(" - ${path}")
endforeach()

execute_process(COMMAND tree /usr/include
                OUTPUT_VARIABLE tree_output
                RESULT_VARIABLE tree_result)

if(tree_result EQUAL 0)
    message("Tree output for /usr/include:\n${tree_output}")
else()
    message("Failed to execute 'tree /usr/include' command")
endif()


find_path(STB_IMAGE_RESIZE2_INCLUDE_DIR stb_image_resize2.h PATH_SUFFIXES stb)
if(STB_IMAGE_RESIZE2_INCLUDE_DIR)
	set(STB_IMAGE_RESIZE_VERSION 2)
else()
	find_path(STB_IMAGE_RESIZE_INCLUDE_DIR stb_image_resize.h PATH_SUFFIXES stb)
	if(STB_IMAGE_RESIZE_INCLUDE_DIR)
		set(STB_IMAGE_RESIZE_VERSION 1)
	endif()
endif()

include(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(
		StbImage
		REQUIRED_VARS STB_IMAGE_INCLUDE_DIR STB_IMAGE_RESIZE_VERSION
)

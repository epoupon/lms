find_package(Eigen3)

find_path(ESSENTIA_INCLUDES essentia/essentia.h)
find_library(ESSENTIA_LIB libessentia.so)

add_library(essentia SHARED IMPORTED)
target_link_libraries(essentia INTERFACE Eigen3::Eigen)
target_include_directories(essentia INTERFACE ${ESSENTIA_INCLUDES})
set_target_properties(essentia
    PROPERTIES
        IMPORTED_LOCATION ${ESSENTIA_LIB}
        INTERFACE_INCLUDE_DIRECTORIES ${ESSENTIA_INCLUDES}
    )

find_package_handle_standard_args(Essentia
    REQUIRED_VARS ESSENTIA_LIB ESSENTIA_INCLUDES
    )

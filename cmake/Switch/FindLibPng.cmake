include(FindPackageHandleStandardArgs)

if(NOT SWITCH)
    message(FATAL_ERROR "This helper can only be used when cross-compiling for the Switch.")
endif()

set(PNG_PATHS ${PNG} $ENV{PNG} ${DEVKITPRO}/portlibs/switch png)

find_path(PNG_INCLUDE_DIR png.h pngconf.h pnglibconf.h
        PATHS ${PNG_PATHS}
        PATH_SUFFIXES include)

find_library(PNG_LIBRARY NAMES libpng.a
        PATHS ${PNG_PATHS}
        PATH_SUFFIXES lib)


set(PNG_INCLUDE_DIRS ${PNG_INCLUDE_DIR})
set(PNG_LIBRARIES ${PNG_LIBRARY})

find_package_handle_standard_args(LibPng DEFAULT_MSG PNG_INCLUDE_DIR PNG_LIBRARY)

mark_as_advanced(PNG_INCLUDE_DIR PNG_LIBRARY)

if(LibPng_FOUND)
    set(PNG ${PNG_INCLUDE_DIR}/..)

    add_library(switch::png STATIC IMPORTED GLOBAL)
    set_target_properties(switch::png PROPERTIES
            IMPORTED_LOCATION "${PNG_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${PNG_INCLUDE_DIR}")
    
    include_directories(${PNG_INCLUDE_DIR})
endif()


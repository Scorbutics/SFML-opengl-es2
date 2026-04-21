include(FindPackageHandleStandardArgs)

if(NOT SWITCH)
    message(FATAL_ERROR "This helper can only be used when cross-compiling for the Switch.")
endif()

set(BZ2_PATHS ${BZ2} $ENV{BZ2} ${DEVKITPRO}/portlibs/switch bz2)
set(BZ2_INCLUDE_DIR ${DEVKITPRO}/portlibs/switch/include)

find_library(BZ2_LIBRARY NAMES libbz2.a
        PATHS ${BZ2_PATHS}
        PATH_SUFFIXES lib)

set(BZ2_INCLUDE_DIRS ${BZ2_INCLUDE_DIR})
set(BZ2_LIBRARIES ${BZ2_LIBRARY})

find_package_handle_standard_args(LibBz2 DEFAULT_MSG BZ2_INCLUDE_DIR BZ2_LIBRARY)
set(BZ2_FOUND ${LibBz2_FOUND})  # Compatibility alias

if(BZ2_FOUND)
    set(BZ2 ${BZ2_INCLUDE_DIR}/..)

    add_library(switch::bz2 STATIC IMPORTED GLOBAL)
    set_target_properties(switch::bz2 PROPERTIES
            IMPORTED_LOCATION "${BZ2_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${BZ2_INCLUDE_DIR}")
endif()
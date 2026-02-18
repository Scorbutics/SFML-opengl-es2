## A CMake helper to find GL libs on the system.

include(FindPackageHandleStandardArgs)

if(NOT SWITCH)
    message(FATAL_ERROR "This helper can only be used when cross-compiling for the Switch.")
endif()

set(VORBIS_PATHS ${VORBIS} $ENV{VORBIS} ${DEVKITPRO}/portlibs/switch vorbis)
set(VORBIS_INCLUDE_DIR ${DEVKITPRO}/portlibs/switch/include)

find_library(VORBIS_LIBRARY NAMES libvorbis.a
        PATHS ${VORBIS_PATHS}
        PATH_SUFFIXES lib)

set(VORBIS_INCLUDE_DIRS ${VORBIS_INCLUDE_DIR})
set(VORBIS_LIBRARIES ${VORBIS_LIBRARY})

find_package_handle_standard_args(LibVorbis DEFAULT_MSG VORBIS_INCLUDE_DIR VORBIS_LIBRARY)
set(VORBIS_FOUND ${LibVorbis_FOUND})  # Compatibility alias

if(VORBIS_FOUND)
    set(VORBIS ${VORBIS_INCLUDE_DIR}/..)

    add_library(switch::vorbis STATIC IMPORTED GLOBAL)
    set_target_properties(switch::vorbis PROPERTIES
            IMPORTED_LOCATION "${VORBIS_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${VORBIS_INCLUDE_DIR}")
endif()

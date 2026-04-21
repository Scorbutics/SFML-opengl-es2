include(FindPackageHandleStandardArgs)

if(NOT SWITCH)
    message(FATAL_ERROR "This helper can only be used when cross-compiling for the Switch.")
endif()

set(HARFBUZZ_PATHS ${HARFBUZZ} $ENV{HARFBUZZ} ${DEVKITPRO}/portlibs/switch harfbuzz)
set(HARFBUZZ_INCLUDE_DIR ${DEVKITPRO}/portlibs/switch/include/harfbuzz)

find_library(HARFBUZZ_LIBRARY NAMES libharfbuzz.a
        PATHS ${HARFBUZZ_PATHS}
        PATH_SUFFIXES lib)

set(HARFBUZZ_INCLUDE_DIRS ${HARFBUZZ_INCLUDE_DIR})
set(HARFBUZZ_LIBRARIES ${HARFBUZZ_LIBRARY})

find_package_handle_standard_args(LibHarfbuzz DEFAULT_MSG HARFBUZZ_INCLUDE_DIR HARFBUZZ_LIBRARY)
set(HARFBUZZ_FOUND ${LibHarfbuzz_FOUND})

if(HARFBUZZ_FOUND)
    set(HARFBUZZ ${HARFBUZZ_INCLUDE_DIR}/..)

    add_library(switch::harfbuzz STATIC IMPORTED GLOBAL)
    set_target_properties(switch::harfbuzz PROPERTIES
            IMPORTED_LOCATION "${HARFBUZZ_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${HARFBUZZ_INCLUDE_DIR}")
endif()
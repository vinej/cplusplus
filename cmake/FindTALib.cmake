# Tiny finder for TA-Lib installed via vcpkg (or any standard prefix).
# Provides the imported target TALib::TALib.

find_path(TALib_INCLUDE_DIR
    NAMES ta-lib/ta_libc.h ta_libc.h
)

find_library(TALib_LIBRARY
    NAMES ta-lib ta_lib ta_libc ta-lib-static
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TALib
    REQUIRED_VARS TALib_LIBRARY TALib_INCLUDE_DIR
)

if(TALib_FOUND AND NOT TARGET TALib::TALib)
    add_library(TALib::TALib UNKNOWN IMPORTED)
    set_target_properties(TALib::TALib PROPERTIES
        IMPORTED_LOCATION "${TALib_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${TALib_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(TALib_INCLUDE_DIR TALib_LIBRARY)

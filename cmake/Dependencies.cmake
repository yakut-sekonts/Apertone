include(FetchContent)

# Runtime sources are immutable releases, verified before extraction.
FetchContent_Declare(libraw
    URL https://www.libraw.org/data/LibRaw-0.21.5.tar.gz
    URL_HASH SHA256=a74a2e68303d3b9219f82318f935b28c5c4abd7f2c9f7dbf8faa4997c9038305)
FetchContent_Declare(sqlite
    URL https://sqlite.org/2025/sqlite-amalgamation-3500400.zip
    URL_HASH SHA256=1d3049dd0f830a025a53105fc79fd2ab9431aea99e137809d064d8ee8356b032)
FetchContent_Declare(lcms
    URL https://codeload.github.com/mm2/Little-CMS/tar.gz/refs/tags/lcms2.17
    URL_HASH SHA256=6e6f6411db50e85ae8ff7777f01b2da0614aac13b7b9fcbea66dc56a1bc71418)
FetchContent_MakeAvailable(libraw sqlite lcms)

# LibRaw's release does not ship a CMake project. Its src directory contains
# the LGPL/CDDL core, with no optional GPL demosaic packs or external SDKs.
include("${CMAKE_CURRENT_LIST_DIR}/LibRawSources.cmake")
add_library(luma_libraw SHARED ${raw_sources})
target_include_directories(luma_libraw SYSTEM PUBLIC "${libraw_SOURCE_DIR}")
target_compile_definitions(luma_libraw PRIVATE LIBRAW_NODLL)
set_target_properties(luma_libraw PROPERTIES OUTPUT_NAME luma_raw)

add_library(luma_sqlite STATIC "${sqlite_SOURCE_DIR}/sqlite3.c")
target_include_directories(luma_sqlite SYSTEM PUBLIC "${sqlite_SOURCE_DIR}")
target_compile_definitions(luma_sqlite PRIVATE SQLITE_THREADSAFE=1 SQLITE_DQS=0
    SQLITE_DEFAULT_FOREIGN_KEYS=1 SQLITE_ENABLE_FTS5)

file(GLOB lcms_sources CONFIGURE_DEPENDS "${lcms_SOURCE_DIR}/src/*.c")
add_library(luma_lcms STATIC ${lcms_sources})
target_include_directories(luma_lcms SYSTEM PUBLIC "${lcms_SOURCE_DIR}/include")

find_package(Qt6 6.10.3 EXACT REQUIRED COMPONENTS Core Gui Quick QuickControls2)

# Only calibration data are consumed; Lensfun library/build code is not compiled.
FetchContent_Declare(lens_database
    URL https://codeload.github.com/lensfun/lensfun/tar.gz/refs/tags/v0.3.4
    URL_HASH SHA256=dafb39c08ef24a0e2abd00d05d7341b1bf1f0c38bfcd5a4c69cf5f0ecb6db112
    SOURCE_SUBDIR luma-data-only)
FetchContent_MakeAvailable(lens_database)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/generated/LensDatabase.h"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/compile-lens-database.py"
        "${lens_database_SOURCE_DIR}/data/db" "${CMAKE_BINARY_DIR}/generated/LensDatabase.h"
    DEPENDS "${CMAKE_SOURCE_DIR}/scripts/compile-lens-database.py"
    VERBATIM)

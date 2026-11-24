include(FetchContent)

# Lightroom Classic stores grayscale mask tiles as DNG JPEG XL codestreams.
FetchContent_Declare(apertone_jpegxl
    GIT_REPOSITORY https://github.com/libjxl/libjxl.git
    GIT_TAG v0.13.0 GIT_SHALLOW TRUE
    GIT_CONFIG core.symlinks=false
    GIT_SUBMODULES third_party/brotli third_party/highway third_party/skcms
    SOURCE_SUBDIR aperture-data-only)
FetchContent_MakeAvailable(apertone_jpegxl)
execute_process(COMMAND git -C "${apertone_jpegxl_SOURCE_DIR}" rev-parse HEAD
    OUTPUT_VARIABLE aperture_jxl_revision OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
if(NOT aperture_jxl_revision STREQUAL "5a7da83447212fe4900fc29ce97430b881caa67e")
    message(FATAL_ERROR "Unexpected JPEG XL source revision.")
endif()
function(aperture_add_jpegxl)
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_TESTING OFF)
    set(JPEGXL_ENABLE_SKCMS ON CACHE INTERNAL "Use the pinned JPEG XL color library")
    foreach(feature TOOLS FUZZERS DEVTOOLS DOXYGEN MANPAGES BENCHMARK EXAMPLES JNI OPENEXR SJPEG TCMALLOC TRANSCODE_JPEG)
        set(JPEGXL_ENABLE_${feature} OFF CACHE INTERNAL "Apertone requires the JPEG XL decoder")
    endforeach()
    add_subdirectory("${apertone_jpegxl_SOURCE_DIR}" "${apertone_jpegxl_BINARY_DIR}" EXCLUDE_FROM_ALL)
    if(MINGW)
        # The pinned GCC/MinGW toolchain intermittently faults in the AVX2
        # JPEG XL group decoder. Use Highway's Windows x64 baseline here.
        target_compile_definitions(jxl_base INTERFACE HWY_COMPILE_ONLY_STATIC)
    endif()
endfunction()
aperture_add_jpegxl()

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
if(WIN32)
    target_compile_definitions(luma_libraw PUBLIC LIBRAW_WIN32_DLLDEFS
        PRIVATE LIBRAW_BUILDLIB _USE_MATH_DEFINES)
    target_link_libraries(luma_libraw PRIVATE ws2_32)
else()
    target_compile_definitions(luma_libraw PRIVATE LIBRAW_NODLL)
endif()
set_target_properties(luma_libraw PROPERTIES OUTPUT_NAME luma_raw)

add_library(luma_sqlite STATIC "${sqlite_SOURCE_DIR}/sqlite3.c")
target_include_directories(luma_sqlite SYSTEM PUBLIC "${sqlite_SOURCE_DIR}")
target_compile_definitions(luma_sqlite PRIVATE SQLITE_THREADSAFE=1 SQLITE_DQS=0
    SQLITE_DEFAULT_FOREIGN_KEYS=1 SQLITE_ENABLE_FTS5)

file(GLOB lcms_sources CONFIGURE_DEPENDS "${lcms_SOURCE_DIR}/src/*.c")
add_library(luma_lcms STATIC ${lcms_sources})
target_include_directories(luma_lcms SYSTEM PUBLIC "${lcms_SOURCE_DIR}/include")

find_package(Qt6 6.10.3 EXACT REQUIRED COMPONENTS Core Gui Network PrintSupport Quick QuickControls2)
if(WIN32)
    find_package(Qt6 6.10.3 EXACT REQUIRED COMPONENTS Xml)
    # Use the matching Windows Portable Devices headers missing from the Qt MinGW distribution.
    # Keep the rest of its Windows ABI headers in place.
    set(apertone_wpd_include "${CMAKE_BINARY_DIR}/generated/wpd")
    file(MAKE_DIRECTORY "${apertone_wpd_include}")
    set(apertone_wpd_upstream "https://raw.githubusercontent.com/mingw-w64/mingw-w64/v12.0.0/mingw-w64-headers/include")
    function(apertone_wpd_header name expected)
        set(header "${apertone_wpd_include}/${name}")
        if(EXISTS "${header}")
            file(SHA256 "${header}" actual)
            if(actual STREQUAL expected)
                return()
            endif()
        endif()
        file(DOWNLOAD "${apertone_wpd_upstream}/${name}" "${header}" EXPECTED_HASH "SHA256=${expected}"
            TLS_VERIFY ON TIMEOUT 30)
    endfunction()
    apertone_wpd_header(portabledevice.h 893f0b515a4a216ada26a6f60bab1232dcc453fb65a6f52c91fe670835225aaf)
    apertone_wpd_header(portabledeviceapi.h ab15dd0867eced15cd12e98f809312ffa1c42fd1840b5ff31cf2c59209751d20)
    apertone_wpd_header(portabledevicetypes.h 6e82dc790c4a49245b498748f37958062e51bfead34348c188f5a3dfaad1198c)
endif()

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
        "${CMAKE_SOURCE_DIR}/resources/lens/industar-61.xml"
    VERBATIM)

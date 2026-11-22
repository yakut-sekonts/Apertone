include(FetchContent)
include(ExternalProject)

FetchContent_Declare(apertone_jpeg
    URL https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/3.1.4/libjpeg-turbo-3.1.4.tar.gz
    URL_HASH SHA256=e23d3ebb2c6ee4d0e2a5823dbb55b614845df5ea3435c956fed5cf04041a87ad
    SOURCE_SUBDIR aperture-source-only)
FetchContent_Declare(apertone_uhdr
    URL https://codeload.github.com/google/libultrahdr/tar.gz/refs/tags/v2.0.2
    URL_HASH SHA256=aa8d193bb887c348c419780511dd03b374f4e07af8812b6d3f80c8537cf1ef2c)

function(apertone_configure_ultrahdr)
    FetchContent_MakeAvailable(apertone_jpeg)
    # libjpeg-turbo requires its own top-level CMake project.
    set(jpeg_build "${CMAKE_BINARY_DIR}/jpeg-hdr")
    set(JPEG_LIBRARY "${jpeg_build}/libjpeg.a")
    set(JPEG_INCLUDE_DIR "${apertone_jpeg_SOURCE_DIR}/src")
    ExternalProject_Add(luma_hdr_jpeg
        SOURCE_DIR "${apertone_jpeg_SOURCE_DIR}"
        BINARY_DIR "${jpeg_build}"
        DOWNLOAD_COMMAND ""
        CMAKE_ARGS
            "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
            "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
            -DCMAKE_BUILD_TYPE=Release -DENABLE_SHARED=OFF -DENABLE_STATIC=ON
            -DWITH_SIMD=OFF -DWITH_TURBOJPEG=OFF -DWITH_TOOLS=OFF -DWITH_TESTS=OFF
        BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR> --target jpeg-static
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS "${JPEG_LIBRARY}")

    set(BUILD_SHARED_LIBS OFF)
    set(UHDR_BUILD_EXAMPLES OFF)
    set(UHDR_BUILD_DEPS OFF)
    set(UHDR_BUILD_TESTS OFF)
    set(UHDR_ENABLE_HEIF OFF)
    set(UHDR_ENABLE_GLES OFF)
    set(UHDR_ENABLE_INSTALL OFF)
    set(UHDR_WRITE_XMP ON)
    set(UHDR_WRITE_ISO ON)
    FetchContent_MakeAvailable(apertone_uhdr)
    target_include_directories(core PRIVATE "${jpeg_build}")
    add_dependencies(core luma_hdr_jpeg)
    install(FILES "${apertone_jpeg_SOURCE_DIR}/LICENSE.md"
        "${apertone_jpeg_SOURCE_DIR}/README.ijg" DESTINATION licenses/libjpeg-turbo)
    install(FILES "${apertone_uhdr_SOURCE_DIR}/LICENSE" DESTINATION licenses/libultrahdr)
endfunction()

apertone_configure_ultrahdr()

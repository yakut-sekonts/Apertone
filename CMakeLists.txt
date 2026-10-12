cmake_minimum_required(VERSION 3.25)
if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 14.0 CACHE STRING "Minimum supported macOS" FORCE)
endif()
project(Luma VERSION 0.1.0 LANGUAGES C CXX OBJCXX)

if(NOT APPLE OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
    message(FATAL_ERROR "Luma currently supports macOS Apple Silicon only.")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OSX_ARCHITECTURES arm64)
set(CMAKE_OSX_DEPLOYMENT_TARGET 14.0 CACHE STRING "Minimum supported macOS")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(EXISTS "${CMAKE_SOURCE_DIR}/.tools/qt/6.10.3/macos")
    list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/.tools/qt/6.10.3/macos")
endif()
include(cmake/Dependencies.cmake)

add_library(luma_core STATIC
    core/catalog/Catalog.cpp core/raw/RawDecoder.cpp core/image/Bayer.cpp
    core/render/CpuRenderer.cpp core/color/DisplayTransform.cpp core/jobs/JobManager.cpp)
target_include_directories(luma_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(luma_core PUBLIC luma_libraw luma_sqlite luma_lcms)
target_compile_options(luma_core PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_library(luma_macos STATIC platform/macos/DisplayProfile.mm platform/macos/FileHasher.cpp)
target_include_directories(luma_macos PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(luma_macos PRIVATE "-framework AppKit" "-framework ColorSync")
target_compile_options(luma_macos PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_library(luma_services STATIC apps/shared/SessionService.cpp)
target_include_directories(luma_services PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(luma_services PUBLIC luma_core luma_macos)
target_compile_options(luma_services PRIVATE -Wall -Wextra -Wpedantic -Werror)

qt_standard_project_setup(REQUIRES 6.10)
qt_add_executable(Luma MACOSX_BUNDLE
    apps/desktop/main.cpp apps/desktop/EditorController.cpp apps/desktop/PreviewProvider.cpp)
qt_add_qml_module(Luma URI Luma.Desktop VERSION 1.0
    QML_FILES apps/desktop/qml/Main.qml apps/desktop/qml/MetadataRow.qml)
target_link_libraries(Luma PRIVATE luma_services Qt6::Quick Qt6::QuickControls2)
target_compile_options(Luma PRIVATE -Wall -Wextra -Wpedantic -Werror)
set_target_properties(Luma PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "app.luma.desktop"
    MACOSX_BUNDLE_BUNDLE_NAME Luma
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/platform/macos/Info.plist.in")

add_executable(luma_cli apps/cli/main.cpp)
set_target_properties(luma_cli PROPERTIES OUTPUT_NAME luma
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_link_libraries(luma_cli PRIVATE luma_services)
target_compile_options(luma_cli PRIVATE -Wall -Wextra -Wpedantic -Werror)

include(CTest)
if(BUILD_TESTING)
    add_executable(luma_tests tests/unit/CoreTests.cpp)
    target_link_libraries(luma_tests PRIVATE luma_services)
    target_compile_options(luma_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME core-unit COMMAND luma_tests)
    add_test(NAME qml-shell COMMAND Luma --smoke-test --catalog "${CMAKE_BINARY_DIR}/smoke.sqlite")
    set_tests_properties(qml-shell PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 30)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME synthetic-raw-restart COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_SOURCE_DIR}/tests/integration/restart.py" "$<TARGET_FILE:luma_cli>" synthetic)
    add_test(NAME nikon-d7500-restart COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_SOURCE_DIR}/tests/integration/restart.py" "$<TARGET_FILE:luma_cli>" private)
    set_tests_properties(nikon-d7500-restart PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 180)
    set_tests_properties(synthetic-raw-restart PROPERTIES TIMEOUT 90)
endif()

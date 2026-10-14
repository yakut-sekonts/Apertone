cmake_minimum_required(VERSION 3.25)
if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 14.0 CACHE STRING "Minimum supported macOS" FORCE)
endif()
project(Luma VERSION 0.4.8 LANGUAGES C CXX OBJCXX)

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
include(cmake/Sparkle.cmake)

add_library(luma_core STATIC
    "${CMAKE_BINARY_DIR}/generated/LensDatabase.h"
    core/math/Homography.cpp core/edits/TransformSettings.cpp core/render/TransformMapping.cpp core/render/PerspectiveCorrection.cpp
    core/lens/LensProfiles.cpp core/lens/LensCorrection.cpp core/edits/LensSettings.cpp
    core/catalog/TransformMigration.cpp core/catalog/LensMigration.cpp core/catalog/CropMigration.cpp core/catalog/NoiseReductionMigration.cpp core/catalog/SharpeningMigration.cpp core/catalog/BatchEdits.cpp core/library/LibraryTypes.cpp core/catalog/LibraryCatalog.cpp core/catalog/Catalog.cpp core/raw/RawDecoder.cpp core/image/Bayer.cpp
    core/presets/SettingsSubset.cpp core/render/NoiseReduction.cpp core/render/Sharpening.cpp core/render/CropGeometry.cpp core/render/CpuRenderer.cpp core/render/DevelopPipeline.cpp core/render/AdvancedColor.cpp core/edits/CropSettings.cpp core/edits/EditDocument.cpp core/export/JpegICC.cpp
    core/color/DisplayTransform.cpp core/jobs/JobManager.cpp)
target_include_directories(luma_core PRIVATE "${CMAKE_BINARY_DIR}/generated")
target_include_directories(luma_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(luma_core PUBLIC luma_libraw luma_sqlite luma_lcms)
target_compile_options(luma_core PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_library(luma_macos STATIC platform/macos/DisplayProfile.mm platform/macos/FileHasher.cpp platform/macos/ImageExporter.mm)
target_include_directories(luma_macos PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(luma_macos PRIVATE luma_lcms "-framework AppKit" "-framework ColorSync" "-framework ImageIO")
target_compile_options(luma_macos PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_library(luma_services STATIC apps/shared/SessionService.cpp apps/shared/LibraryService.cpp)
target_include_directories(luma_services PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(luma_services PUBLIC luma_core luma_macos)
target_compile_options(luma_services PRIVATE -Wall -Wextra -Wpedantic -Werror)

qt_standard_project_setup(REQUIRES 6.10)
qt_add_executable(Luma MACOSX_BUNDLE
    apps/desktop/BatchController.cpp apps/desktop/BatchController.h
    apps/desktop/PresetController.cpp apps/desktop/PresetController.h apps/shared/PresetStore.cpp apps/shared/BuiltinPresets.cpp apps/desktop/EditorPresets.cpp
    apps/desktop/UpdateController.cpp apps/desktop/UpdateController.h platform/macos/Updater.mm
    apps/desktop/main.cpp apps/desktop/EditorController.cpp apps/desktop/EditorTransform.cpp apps/desktop/EditorLens.cpp apps/desktop/EditorCrop.cpp apps/desktop/EditorDetail.cpp apps/desktop/EditorAdvanced.cpp apps/desktop/PreviewProvider.cpp apps/desktop/ThumbnailProvider.cpp apps/desktop/LibraryModel.cpp apps/desktop/LibraryAcceptance.cpp)
qt_add_qml_module(Luma URI Luma.Desktop VERSION 1.0
    QML_FILES apps/desktop/qml/TransformPanel.qml apps/desktop/qml/TransformOverlay.qml apps/desktop/qml/LensPanel.qml apps/desktop/qml/CropPanel.qml apps/desktop/qml/CropOverlay.qml apps/desktop/qml/DetailPanel.qml apps/desktop/qml/BatchSettings.qml apps/desktop/qml/PresetPanel.qml apps/desktop/qml/Main.qml apps/desktop/qml/UpdateBell.qml apps/desktop/qml/MetadataRow.qml apps/desktop/qml/DevelopSlider.qml apps/desktop/qml/CurvesPanel.qml apps/desktop/qml/HSLPanel.qml apps/desktop/qml/LibraryPane.qml apps/desktop/qml/PhotoCard.qml)
target_link_libraries(Luma PRIVATE luma_services Qt6::Quick Qt6::QuickControls2 luma_sparkle)
set_source_files_properties(platform/macos/Updater.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
set_property(TARGET Luma APPEND PROPERTY BUILD_RPATH "${sparkle_SOURCE_DIR}")
add_custom_command(TARGET Luma POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_DIR:Luma>/Contents/Frameworks"
    COMMAND /usr/bin/ditto "${sparkle_SOURCE_DIR}/Sparkle.framework"
        "$<TARGET_BUNDLE_DIR:Luma>/Contents/Frameworks/Sparkle.framework"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_DIR:Luma>/Contents/Resources/ThirdParty"
    COMMAND ${CMAKE_COMMAND} -E copy "${sparkle_SOURCE_DIR}/LICENSE"
        "$<TARGET_BUNDLE_DIR:Luma>/Contents/Resources/ThirdParty/Sparkle-LICENSE.txt")
target_compile_options(Luma PRIVATE -Wall -Wextra -Wpedantic -Werror)
set_target_properties(Luma PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "app.luma.desktop"
    MACOSX_BUNDLE_BUNDLE_NAME Luma
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/platform/macos/Info.plist.in")

add_executable(luma_cli apps/cli/main.cpp apps/cli/LibraryCommands.cpp)
set_target_properties(luma_cli PROPERTIES OUTPUT_NAME luma
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_link_libraries(luma_cli PRIVATE luma_services)
target_compile_options(luma_cli PRIVATE -Wall -Wextra -Wpedantic -Werror)

include(CTest)
if(BUILD_TESTING)
    qt_add_executable(luma_update_tests tests/unit/UpdateTests.cpp apps/desktop/UpdateController.cpp apps/desktop/UpdateController.h)
    target_include_directories(luma_update_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(luma_update_tests PRIVATE Qt6::Core)
    target_compile_options(luma_update_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME update-restart-guard COMMAND luma_update_tests)
    qt_add_executable(luma_batch_tests tests/unit/BatchTests.cpp apps/desktop/BatchController.cpp
        apps/desktop/BatchController.h apps/desktop/LibraryModel.cpp
        apps/desktop/UpdateController.cpp apps/desktop/UpdateController.h
        apps/shared/PresetStore.cpp apps/shared/BuiltinPresets.cpp apps/desktop/PresetController.cpp apps/desktop/PresetController.h
        apps/desktop/EditorController.cpp apps/desktop/EditorTransform.cpp apps/desktop/EditorLens.cpp apps/desktop/EditorCrop.cpp apps/desktop/EditorDetail.cpp apps/desktop/EditorAdvanced.cpp apps/desktop/EditorPresets.cpp
        apps/desktop/PreviewProvider.cpp)
    target_link_libraries(luma_batch_tests PRIVATE luma_services Qt6::Quick Qt6::QuickControls2)
    target_compile_options(luma_batch_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    qt_add_executable(luma_preset_tests tests/unit/PresetTests.cpp tests/unit/FilterEditorTests.cpp tests/unit/CropEditorTests.cpp tests/unit/LensEditorTests.cpp tests/unit/TransformEditorTests.cpp apps/shared/PresetStore.cpp apps/shared/BuiltinPresets.cpp
        apps/desktop/PresetController.cpp apps/desktop/PresetController.h
        apps/desktop/EditorController.cpp apps/desktop/EditorTransform.cpp apps/desktop/EditorLens.cpp apps/desktop/EditorCrop.cpp apps/desktop/EditorDetail.cpp apps/desktop/EditorAdvanced.cpp apps/desktop/EditorPresets.cpp
        apps/desktop/PreviewProvider.cpp)
    target_link_libraries(luma_preset_tests PRIVATE luma_services Qt6::Quick Qt6::QuickControls2)
    target_compile_options(luma_preset_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_executable(luma_filter_tests tests/unit/FilterTests.cpp apps/shared/PresetStore.cpp apps/shared/BuiltinPresets.cpp)
    target_link_libraries(luma_filter_tests PRIVATE luma_services Qt6::Gui)
    target_compile_options(luma_filter_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME builtin-filter-photographic-quality COMMAND luma_filter_tests "${CMAKE_SOURCE_DIR}/tests/assets/filters" "${CMAKE_BINARY_DIR}/qa/filters")
    set_tests_properties(builtin-filter-photographic-quality PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
    add_executable(luma_transform_tests tests/unit/TransformTests.cpp)
    target_link_libraries(luma_transform_tests PRIVATE luma_services)
    target_compile_options(luma_transform_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME perspective-transform-unit COMMAND luma_transform_tests)
    add_executable(luma_lens_tests tests/unit/LensTests.cpp)
    target_link_libraries(luma_lens_tests PRIVATE luma_services)
    target_compile_options(luma_lens_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME lens-calibration-unit COMMAND luma_lens_tests)
    add_executable(luma_crop_tests tests/unit/CropTests.cpp)
    target_link_libraries(luma_crop_tests PRIVATE luma_services)
    target_compile_options(luma_crop_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME crop-geometry-unit COMMAND luma_crop_tests)
    add_executable(luma_noise_tests tests/unit/NoiseReductionTests.cpp)
    target_link_libraries(luma_noise_tests PRIVATE luma_services)
    target_compile_options(luma_noise_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME noise-reduction-unit COMMAND luma_noise_tests)
    add_executable(luma_sharpening_tests tests/unit/SharpeningTests.cpp)
    target_link_libraries(luma_sharpening_tests PRIVATE luma_services)
    target_compile_options(luma_sharpening_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME sharpening-unit COMMAND luma_sharpening_tests)
    add_executable(luma_tests tests/unit/CoreTests.cpp)
    target_link_libraries(luma_tests PRIVATE luma_services)
    target_compile_options(luma_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME core-unit COMMAND luma_tests)
    add_executable(luma_advanced_tests tests/unit/AdvancedTests.cpp)
    target_link_libraries(luma_advanced_tests PRIVATE luma_services)
    target_compile_options(luma_advanced_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME advanced-color-unit COMMAND luma_advanced_tests)
    add_executable(luma_library_tests tests/unit/LibraryTests.cpp apps/desktop/LibraryModel.cpp)
    target_link_libraries(luma_library_tests PRIVATE luma_services Qt6::Core)
    target_compile_options(luma_library_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME qml-shell COMMAND Luma --smoke-test --catalog "${CMAKE_BINARY_DIR}/smoke.sqlite")
    set_tests_properties(qml-shell PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 30)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME batch-settings-atomic-history COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_SOURCE_DIR}/tests/integration/batch.py" "$<TARGET_FILE:luma_batch_tests>")
    set_tests_properties(batch-settings-atomic-history PROPERTIES TIMEOUT 180)
    add_test(NAME presets-storage-preview-history COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_SOURCE_DIR}/tests/integration/presets.py" "$<TARGET_FILE:luma_preset_tests>")
    set_tests_properties(presets-storage-preview-history PROPERTIES TIMEOUT 180)
    add_test(NAME signed-update-feed COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_SOURCE_DIR}/tests/integration/appcast.py")
    set_tests_properties(signed-update-feed PROPERTIES TIMEOUT 180)
    add_test(NAME release-publisher COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_SOURCE_DIR}/tests/integration/release_publisher.py")
    add_test(NAME release-dmg-recovery COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_SOURCE_DIR}/tests/integration/release_dmg.py")
    add_test(NAME release-build-verification COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_SOURCE_DIR}/tests/integration/release_build.py")
    add_test(NAME library-import-restart COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_SOURCE_DIR}/tests/integration/library.py" "$<TARGET_FILE:luma_cli>" "$<TARGET_FILE:luma_library_tests>")
    set_tests_properties(library-import-restart PROPERTIES TIMEOUT 180)
    add_test(NAME synthetic-raw-restart COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_SOURCE_DIR}/tests/integration/restart.py" "$<TARGET_FILE:luma_cli>" synthetic)
    add_test(NAME develop-export-restart COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_SOURCE_DIR}/tests/integration/develop.py" "$<TARGET_FILE:luma_cli>")
    add_test(NAME transform-export-migration COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_SOURCE_DIR}/tests/integration/transform.py" "$<TARGET_FILE:luma_cli>")
    set_tests_properties(transform-export-migration PROPERTIES TIMEOUT 300)
    add_test(NAME lens-export-migration COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_SOURCE_DIR}/tests/integration/lens.py" "$<TARGET_FILE:luma_cli>")
    set_tests_properties(lens-export-migration PROPERTIES TIMEOUT 300)
    add_test(NAME crop-export-migration COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_SOURCE_DIR}/tests/integration/crop.py" "$<TARGET_FILE:luma_cli>")
    set_tests_properties(crop-export-migration PROPERTIES TIMEOUT 300)
    add_test(NAME noise-export-migration COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_SOURCE_DIR}/tests/integration/noise_reduction.py" "$<TARGET_FILE:luma_cli>")
    set_tests_properties(noise-export-migration PROPERTIES TIMEOUT 300)
    add_test(NAME sharpening-export-migration COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_SOURCE_DIR}/tests/integration/sharpening.py" "$<TARGET_FILE:luma_cli>")
    set_tests_properties(sharpening-export-migration PROPERTIES TIMEOUT 300)
    add_test(NAME advanced-export-restart COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_SOURCE_DIR}/tests/integration/advanced.py" "$<TARGET_FILE:luma_cli>")
    set_tests_properties(advanced-export-restart PROPERTIES TIMEOUT 240)
    set_tests_properties(develop-export-restart PROPERTIES TIMEOUT 180)
    add_test(NAME nikon-d7500-restart COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_SOURCE_DIR}/tests/integration/restart.py" "$<TARGET_FILE:luma_cli>" private)
    set_tests_properties(nikon-d7500-restart PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 180)
    set_tests_properties(synthetic-raw-restart PROPERTIES TIMEOUT 90)
endif()

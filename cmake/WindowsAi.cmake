include(FetchContent)

FetchContent_Declare(apertone_ort
    URL https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime/1.23.2/microsoft.ml.onnxruntime.1.23.2.nupkg
    URL_HASH SHA256=25fe172f7fcdf34f2b5b02c8b997f6b88abc282956849d4af4335af23d0c4e4a
    DOWNLOAD_NAME onnxruntime.zip
    SOURCE_SUBDIR aperture-source-only)

set(LUMA_WINDOWS_MODEL_CACHE "" CACHE PATH "Directory containing previously downloaded Windows model files")

function(apertone_stage_model name url hash)
    set(output "${CMAKE_BINARY_DIR}/Models/${name}")
    if(EXISTS "${output}")
        file(SHA256 "${output}" existing_hash)
        if(existing_hash STREQUAL hash)
            return()
        endif()
    endif()
    if(NOT LUMA_WINDOWS_MODEL_CACHE STREQUAL "" AND EXISTS "${LUMA_WINDOWS_MODEL_CACHE}/${name}")
        file(SHA256 "${LUMA_WINDOWS_MODEL_CACHE}/${name}" cache_hash)
        if(NOT cache_hash STREQUAL hash)
            message(FATAL_ERROR "Hash mismatch in model cache for ${name}.")
        endif()
        file(COPY_FILE "${LUMA_WINDOWS_MODEL_CACHE}/${name}" "${output}.partial")
    else()
        file(DOWNLOAD "${url}" "${output}.partial" EXPECTED_HASH "SHA256=${hash}"
            TLS_VERIFY ON TIMEOUT 240 STATUS status)
        list(GET status 0 result)
        if(NOT result STREQUAL "0")
            message(FATAL_ERROR "Cannot fetch Windows model ${name}: ${status}")
        endif()
    endif()
    file(RENAME "${output}.partial" "${output}")
endfunction()

function(apertone_configure_windows_ai target)
    FetchContent_MakeAvailable(apertone_ort)
    target_include_directories(${target} SYSTEM PRIVATE "${apertone_ort_SOURCE_DIR}/build/native/include")
    set(runtime "${apertone_ort_SOURCE_DIR}/runtimes/win-x64/native")
    foreach(dll onnxruntime.dll onnxruntime_providers_shared.dll)
        file(COPY_FILE "${runtime}/${dll}" "${CMAKE_BINARY_DIR}/${dll}" ONLY_IF_DIFFERENT)
        install(FILES "${runtime}/${dll}" DESTINATION .)
    endforeach()
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/Models")
    apertone_stage_model(skyseg.onnx
        https://huggingface.co/JianyuanWang/skyseg/resolve/3ba8c6df1d9ba9ff26f637c7ba9568ac11a9aa7f/skyseg.onnx
        ab9c34c64c3d821220a2886a4a06da4642ffa14d5b30e8d5339056a089aa1d39)
    apertone_stage_model(u2netp.onnx
        https://github.com/danielgatis/rembg/releases/download/v0.0.0/u2netp.onnx
        309c8469258dda742793dce0ebea8e6dd393174f89934733ecc8b14c76f4ddd8)
    apertone_stage_model(pphumanseg.onnx
        https://media.githubusercontent.com/media/opencv/opencv_zoo/48b10fa505aeb51fbd127fc60a5c660040e823b7/models/human_segmentation_pphumanseg/human_segmentation_pphumanseg_2023mar.onnx
        552d8a984054e59b5d773d24b9b12022b22046ceb2bbc4c9aaeaceb36a9ddf24)
    apertone_stage_model(u2net-LICENSE.txt
        https://raw.githubusercontent.com/xuebinqin/U-2-Net/ac7e1c817ecab7c7dff5ce6b1abba61cd213ff29/LICENSE
        c71d239df91726fc519c6eb72d318ec65820627232b2f796219e87dcf35d0ab4)
    apertone_stage_model(pphumanseg-LICENSE.txt
        https://raw.githubusercontent.com/opencv/opencv_zoo/48b10fa505aeb51fbd127fc60a5c660040e823b7/models/human_segmentation_pphumanseg/LICENSE
        d9cf3b547fa4232fb9285f2db52e909f10f00781aaf64a6d0bb90c55149f1363)
    install(DIRECTORY "${CMAKE_BINARY_DIR}/Models/" DESTINATION Models FILES_MATCHING PATTERN "*.onnx")
    install(FILES "${CMAKE_SOURCE_DIR}/resources/models/licenses/SkySeg-LICENSE.txt"
        "${CMAKE_BINARY_DIR}/Models/u2net-LICENSE.txt" "${CMAKE_BINARY_DIR}/Models/pphumanseg-LICENSE.txt"
        DESTINATION licenses/windows-models)
    install(FILES "${apertone_ort_SOURCE_DIR}/LICENSE" "${apertone_ort_SOURCE_DIR}/ThirdPartyNotices.txt"
        DESTINATION licenses/onnxruntime)
endfunction()

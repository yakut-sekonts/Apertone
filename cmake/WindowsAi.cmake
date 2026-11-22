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
    apertone_stage_model(yunet.onnx
        https://media.githubusercontent.com/media/opencv/opencv_zoo/48b10fa505aeb51fbd127fc60a5c660040e823b7/models/face_detection_yunet/face_detection_yunet_2023mar.onnx
        8f2383e4dd3cfbb4553ea8718107fc0423210dc964f9f4280604804ed2552fa4)
    apertone_stage_model(depth-anything-v2.onnx
        https://huggingface.co/onnx-community/depth-anything-v2-small/resolve/4472b7362082ad9968fee890ca0f1e5aca36b93d/onnx/model_quantized.onnx
        fcf51f1b230362b28690bb9d1809bf0431f29cad20534e3f589bd7285547f20d)
    apertone_stage_model(drunet-color.onnx
        https://huggingface.co/synthscript/drunet-color-onnx/resolve/a2b9fccfa27b197f44a3876c567f5e48970c44a7/drunet_color.onnx
        2ae3ab5eb15daac2ee79be984d584b908ce7f0f60b27be87d005f728c2aa0087)
    apertone_stage_model(u2net-LICENSE.txt
        https://raw.githubusercontent.com/xuebinqin/U-2-Net/ac7e1c817ecab7c7dff5ce6b1abba61cd213ff29/LICENSE
        c71d239df91726fc519c6eb72d318ec65820627232b2f796219e87dcf35d0ab4)
    apertone_stage_model(pphumanseg-LICENSE.txt
        https://raw.githubusercontent.com/opencv/opencv_zoo/48b10fa505aeb51fbd127fc60a5c660040e823b7/models/human_segmentation_pphumanseg/LICENSE
        d9cf3b547fa4232fb9285f2db52e909f10f00781aaf64a6d0bb90c55149f1363)
    apertone_stage_model(yunet-LICENSE.txt
        https://raw.githubusercontent.com/opencv/opencv_zoo/48b10fa505aeb51fbd127fc60a5c660040e823b7/models/face_detection_yunet/LICENSE
        c83b8120c50ccbd4c4f96edf53141bdd566ebb8f8e9227e415326aa1b1aba958)
    install(DIRECTORY "${CMAKE_BINARY_DIR}/Models/" DESTINATION Models FILES_MATCHING PATTERN "*.onnx")
    install(FILES "${CMAKE_SOURCE_DIR}/resources/models/licenses/SkySeg-LICENSE.txt"
        "${CMAKE_SOURCE_DIR}/resources/models/licenses/DRUNet-LICENSE.txt"
        "${CMAKE_BINARY_DIR}/Models/u2net-LICENSE.txt" "${CMAKE_BINARY_DIR}/Models/pphumanseg-LICENSE.txt"
        "${CMAKE_BINARY_DIR}/Models/yunet-LICENSE.txt"
        DESTINATION licenses/windows-models)
    install(FILES "${CMAKE_SOURCE_DIR}/LICENSE" DESTINATION licenses/windows-models RENAME Depth-Anything-V2-LICENSE.txt)
    install(FILES "${apertone_ort_SOURCE_DIR}/LICENSE" "${apertone_ort_SOURCE_DIR}/ThirdPartyNotices.txt"
        DESTINATION licenses/onnxruntime)
endfunction()

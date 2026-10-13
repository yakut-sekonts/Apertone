include(FetchContent)
FetchContent_Declare(sparkle
    URL https://github.com/sparkle-project/Sparkle/releases/download/2.10.0/Sparkle-2.10.0.tar.xz
    URL_HASH SHA256=c2bf58aa8387266ac179357b1415d6f2635f044da8be41042af32425dae6da0c)
FetchContent_MakeAvailable(sparkle)
add_library(luma_sparkle SHARED IMPORTED)
set_target_properties(luma_sparkle PROPERTIES
    FRAMEWORK TRUE
    IMPORTED_LOCATION "${sparkle_SOURCE_DIR}/Sparkle.framework/Sparkle"
    INTERFACE_INCLUDE_DIRECTORIES "${sparkle_SOURCE_DIR}/Sparkle.framework/Headers")

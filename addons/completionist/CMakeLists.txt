add_library(TesseraCompletionist SHARED EXCLUDE_FROM_ALL src/Plugin.cpp)
target_include_directories(TesseraCompletionist PRIVATE "${PROJECT_SOURCE_DIR}/include" src)
target_link_libraries(TesseraCompletionist PRIVATE CommonLibSSE::CommonLibSSE nlohmann_json::nlohmann_json)
target_compile_definitions(TesseraCompletionist PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_compile_options(TesseraCompletionist PRIVATE /EHsc /utf-8 /Brepro)
target_link_options(TesseraCompletionist PRIVATE /Brepro)
set_target_properties(TesseraCompletionist PROPERTIES PREFIX "" OUTPUT_NAME "TesseraCompletionist")
add_custom_command(TARGET TesseraCompletionist POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/SKSE/Plugins"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:TesseraCompletionist>"
        "${CMAKE_CURRENT_SOURCE_DIR}/SKSE/Plugins/TesseraCompletionist.dll")

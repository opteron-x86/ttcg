cmake_minimum_required(VERSION 3.24)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json" TTCG_MANIFEST)
string(JSON TTCG_PROJECT_VERSION GET "${TTCG_MANIFEST}" version-semver)
string(REGEX REPLACE "-.*$" "" TTCG_BASE_VERSION "${TTCG_PROJECT_VERSION}")
project(TTCG VERSION ${TTCG_BASE_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

if(NOT WIN32)
  enable_testing()
  add_subdirectory(native)
  add_subdirectory(tests)
  return()
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies.json" TTCG_DEPENDENCIES)
string(JSON TTCG_COMMONLIB_COMMIT GET "${TTCG_DEPENDENCIES}" commonlib commit)
string(JSON TTCG_COMMONLIB_VERSION GET "${TTCG_DEPENDENCIES}" commonlib version)
set(TTCG_COMMONLIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/build/deps/commonlib")
if(NOT EXISTS "${TTCG_COMMONLIB_ROOT}/CMakeLists.txt")
  message(FATAL_ERROR "Run python3 tools/setup-dependencies.py first")
endif()
execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY "${TTCG_COMMONLIB_ROOT}"
  OUTPUT_VARIABLE TTCG_COMMONLIB_ACTUAL OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
if(NOT TTCG_COMMONLIB_ACTUAL STREQUAL TTCG_COMMONLIB_COMMIT)
  message(FATAL_ERROR "CommonLib checkout differs from cmake/dependencies.json")
endif()
set(TTCG_COMMONLIB_OVERLAY "${CMAKE_CURRENT_BINARY_DIR}/commonlib-overrides")
include(cmake/CommonLibHashMapPatch.cmake)
set(TTCG_BUILD_LABEL "" CACHE STRING "Optional diagnostic build identifier")
if(NOT TTCG_BUILD_LABEL MATCHES "^[A-Za-z0-9._-]*$")
  message(FATAL_ERROR "TTCG_BUILD_LABEL accepts only letters, digits, dots, underscores and hyphens")
endif()
set(ENABLE_SKYRIM_SE ON CACHE BOOL "Desktop SE support" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "Desktop AE support" FORCE)
set(ENABLE_SKYRIM_VR ON CACHE BOOL "VR support" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "Build CommonLib's own test suite" FORCE)
set(COMMONLIB_PREBUILT OFF CACHE BOOL "Compile CommonLib with Tessera's toolchain" FORCE)
set(COMMONLIB_PREBUILT_DIR "" CACHE PATH "Compile CommonLib with Tessera's toolchain" FORCE)
set(FETCHCONTENT_SOURCE_DIR_HDE64 "${CMAKE_CURRENT_SOURCE_DIR}/build/deps/minhook" CACHE PATH "Locked MinHook sources" FORCE)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
# Compile CommonLib with the lifetime override and Tessera's compiler in CI too.
if(DEFINED ENV{GITHUB_ACTIONS})
  set(TTCG_GITHUB_ACTIONS "$ENV{GITHUB_ACTIONS}")
  unset(ENV{GITHUB_ACTIONS})
endif()
add_subdirectory("${TTCG_COMMONLIB_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/commonlib" EXCLUDE_FROM_ALL)
if(DEFINED TTCG_GITHUB_ACTIONS)
  set(ENV{GITHUB_ACTIONS} "${TTCG_GITHUB_ACTIONS}")
  unset(TTCG_GITHUB_ACTIONS)
endif()
get_target_property(TTCG_COMMONLIB_IMPORTED CommonLibSSE IMPORTED)
if(TTCG_COMMONLIB_IMPORTED)
  message(FATAL_ERROR "CommonLib must be compiled with the lifetime override")
endif()
target_include_directories(CommonLibSSE BEFORE PUBLIC "$<BUILD_INTERFACE:${TTCG_COMMONLIB_OVERLAY}>")
# Compile Tessera and CommonLib against the Windows 10 API.
foreach(TTCG_DEFINITIONS COMPILE_DEFINITIONS INTERFACE_COMPILE_DEFINITIONS)
  get_target_property(TTCG_COMMONLIB_DEFINITIONS CommonLibSSE ${TTCG_DEFINITIONS})
  list(FILTER TTCG_COMMONLIB_DEFINITIONS EXCLUDE REGEX "^(WINVER|_WIN32_WINNT)=")
  list(APPEND TTCG_COMMONLIB_DEFINITIONS WINVER=0x0A00 _WIN32_WINNT=0x0A00)
  set_property(TARGET CommonLibSSE PROPERTY ${TTCG_DEFINITIONS} "${TTCG_COMMONLIB_DEFINITIONS}")
endforeach()
target_compile_options(CommonLibSSE PRIVATE /Brepro)
set(TTCG_OUTPUT_MOD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/runtime-tessera" CACHE PATH "Staged runtime directory")
configure_file(cmake/BuildInfo.h.in generated/BuildInfo.h @ONLY)

# Test menu ownership through the CommonLib container.
add_executable(tessera_commonlib_lifetime_test tests/commonlib_lifetime_test.cpp)
target_include_directories(tessera_commonlib_lifetime_test PRIVATE
  tests/commonlib_stubs "${TTCG_COMMONLIB_OVERLAY}" "${TTCG_COMMONLIB_ROOT}/include")
target_compile_definitions(tessera_commonlib_lifetime_test PRIVATE TESSERA_REQUIRE_HASHMAP_PATCH=1)
target_compile_options(tessera_commonlib_lifetime_test PRIVATE /EHsc /Brepro)

add_library(${PROJECT_NAME} SHARED
  src/Plugin.cpp
  native/skse/Host.cpp
  native/skse/Menu.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
  include
  native/skse
  native/src
  "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  WINVER=0x0A00
  _WIN32_WINNT=0x0A00
  WIN32_LEAN_AND_MEAN
  NOMINMAX
  UNICODE
  _UNICODE
)



target_compile_options(${PROJECT_NAME} PRIVATE
  /Brepro
  /EHsc
  /utf-8
  /permissive-
  /Zc:__cplusplus
  /W4
  /wd4100
  /wd4200
  /wd4201
  /wd4324
  /wd4458
  /wd4625
  /wd4626
  /wd4820
  /wd4996
  /wd5026
  /wd5027
  /wd5039
  /wd5045
  /wd5267
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  CommonLibSSE::CommonLibSSE
  spdlog::spdlog
  nlohmann_json::nlohmann_json
)

set_target_properties(${PROJECT_NAME} PROPERTIES
  PREFIX ""
  OUTPUT_NAME "TTCG"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E make_directory
    "${TTCG_OUTPUT_MOD_DIR}/SKSE/Plugins"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "$<TARGET_FILE:${PROJECT_NAME}>"
    "${TTCG_OUTPUT_MOD_DIR}/SKSE/Plugins/TTCG.dll"
  COMMENT "Staging TTCG.dll for installation and packaging")

# Generate reproducible PE metadata and matching debug symbols.
target_link_options(${PROJECT_NAME} PRIVATE /Brepro /DEBUG:FULL /OPT:REF /OPT:ICF /PDBALTPATH:TTCG.pdb)

# Build the DirectX frontend into TTCG.dll.
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/native/third_party/imgui")
add_library(tessera_imgui STATIC
  ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_draw.cpp ${IMGUI_DIR}/imgui_tables.cpp ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_win32.cpp ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp)
target_compile_definitions(tessera_imgui PRIVATE IMGUI_IMPL_WIN32_DISABLE_GAMEPAD)
target_include_directories(tessera_imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_compile_definitions(tessera_imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS NOMINMAX WIN32_LEAN_AND_MEAN)
foreach(option WEBP_BUILD_ANIM_UTILS WEBP_BUILD_CWEBP WEBP_BUILD_DWEBP WEBP_BUILD_GIF2WEBP WEBP_BUILD_IMG2WEBP WEBP_BUILD_VWEBP WEBP_BUILD_WEBPINFO WEBP_BUILD_EXTRAS WEBP_BUILD_LIBWEBPMUX)
 set(${option} OFF CACHE BOOL "" FORCE)
endforeach()
add_subdirectory(native/third_party/libwebp EXCLUDE_FROM_ALL)
add_library(tessera_frontend STATIC
 native/src/App.cpp native/src/Album.cpp native/src/Packs.cpp
 native/src/GameApp.cpp native/src/GameInspector.cpp native/src/GameAlbum.cpp native/src/GameJournal.cpp native/src/GameRules.cpp
 native/src/GameMatch.cpp native/src/GameSettings.cpp native/src/GameShop.cpp native/src/GameTournaments.cpp
 native/src/Assets.cpp native/src/Material.cpp native/src/RendererDX11.cpp)
target_include_directories(tessera_frontend PUBLIC native/src PRIVATE include )
target_link_libraries(tessera_frontend PUBLIC tessera_imgui nlohmann_json::nlohmann_json webp d3d11 dxgi d3dcompiler dwmapi imm32)
target_compile_options(tessera_frontend PRIVATE /EHsc /utf-8 /Brepro)
target_link_libraries(TTCG PRIVATE tessera_frontend)

# Build the optional Completionist integration as a separate target.
add_subdirectory(addons/completionist EXCLUDE_FROM_ALL)

add_executable(tessera_plugin_probe tests/plugin_probe.c)
add_executable(tessera_dx_state_test native/tests/dx_state_test.cpp native/src/RendererDX11.cpp native/src/Material.cpp)
target_include_directories(tessera_dx_state_test PRIVATE native/src)
target_compile_definitions(tessera_dx_state_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(tessera_dx_state_test PRIVATE tessera_imgui d3d11 dxgi d3dcompiler dwmapi imm32)
foreach(target TTCG CommonLibSSE tessera_frontend tessera_imgui tessera_plugin_probe tessera_dx_state_test tessera_commonlib_lifetime_test)
  target_compile_options(${target} PRIVATE /Z7 /utf-8)
endforeach()
target_compile_options(tessera_dx_state_test PRIVATE /UNDEBUG)

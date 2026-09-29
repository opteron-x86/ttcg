cmake_minimum_required(VERSION 3.24)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json" TTCG_MANIFEST)
string(JSON TTCG_PROJECT_VERSION GET "${TTCG_MANIFEST}" version-semver)
project(TTCG VERSION ${TTCG_PROJECT_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

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
set(ENABLE_SKYRIM_SE ON CACHE BOOL "Desktop SE support" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "Desktop AE support" FORCE)
set(ENABLE_SKYRIM_VR ON CACHE BOOL "VR support" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "Build CommonLib's own test suite" FORCE)
set(COMMONLIB_PREBUILT OFF CACHE BOOL "Compile CommonLib with Tessera's toolchain" FORCE)
set(FETCHCONTENT_SOURCE_DIR_HDE64 "${CMAKE_CURRENT_SOURCE_DIR}/build/deps/minhook" CACHE PATH "Locked MinHook sources" FORCE)
find_package(spdlog CONFIG REQUIRED)
add_subdirectory("${TTCG_COMMONLIB_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/commonlib" EXCLUDE_FROM_ALL)
# CommonLib exports a Windows 7 API floor. Use one consistent Windows 10
# declaration set for it and Tessera, without modifying the locked checkout.
foreach(TTCG_DEFINITIONS COMPILE_DEFINITIONS INTERFACE_COMPILE_DEFINITIONS)
  get_target_property(TTCG_COMMONLIB_DEFINITIONS CommonLibSSE ${TTCG_DEFINITIONS})
  list(FILTER TTCG_COMMONLIB_DEFINITIONS EXCLUDE REGEX "^(WINVER|_WIN32_WINNT)=")
  list(APPEND TTCG_COMMONLIB_DEFINITIONS WINVER=0x0A00 _WIN32_WINNT=0x0A00)
  set_property(TARGET CommonLibSSE PROPERTY ${TTCG_DEFINITIONS} "${TTCG_COMMONLIB_DEFINITIONS}")
endforeach()
target_compile_options(CommonLibSSE PRIVATE /Brepro)
set(TTCG_OUTPUT_MOD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/runtime-meridian" CACHE PATH "Staged runtime directory")
configure_file(cmake/BuildInfo.h.in generated/BuildInfo.h @ONLY)

add_library(${PROJECT_NAME} SHARED
  src/Plugin.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
  include
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

# Omit wall-clock PE timestamps; compiler and SDK identity are recorded separately.
target_link_options(${PROJECT_NAME} PRIVATE /Brepro)

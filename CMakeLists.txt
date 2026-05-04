cmake_minimum_required(VERSION 3.30)
project(terse VERSION 0.1.0 LANGUAGES C CXX)

# C11標準を使用（C99の機能とmixed declarationsをサポート）
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)

# MSVCではiconvが標準で利用できないため、デフォルトでOFF
if(MSVC)
  option(TERSE_USE_SYSTEM_ICONV "Use system iconv for codec conversions" OFF)
else()
  option(TERSE_USE_SYSTEM_ICONV "Use system iconv for codec conversions" ON)
endif()
option(TERSE_ENABLE_TEST_MODE "Enable test mode and mocking features" OFF)

# テストのデフォルト: トップレベルプロジェクトならON、サブプロジェクトならOFF
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  option(TERSE_BUILD_TESTING "Build terse tests" ON)
else()
  option(TERSE_BUILD_TESTING "Build terse tests" OFF)
endif()

include(FetchContent)

if(TERSE_BUILD_TESTING)
  FetchContent_Declare(attest
    GIT_REPOSITORY https://github.com/iwadon/attest.git
    GIT_TAG        0ad168e38ad07802ce07b9b0616d21458799b8ff
  )
  FetchContent_MakeAvailable(attest)
endif()

add_library(terse
  src/terse.c
  src/terse_unicode.c
  src/terse_codec.c
  src/terse_detection.c
  src/terse_event_helpers.c
  src/terse_input.c
  src/terse_style.c
  src/terse_output.c
  src/terse_graphics.c
  src/terse_cursor.c
  src/terse_device.c
  src/terse_state.c
  src/terse_capabilities.c
  src/terse_keyboard.c
  src/terse_test.c
)

include(CheckIncludeFile)

if(MSVC)
  add_compile_options(/source-charset:utf-8)
endif()

# Auto-detect Human68k platform if not explicitly set
if(NOT DEFINED TERSE_PLATFORM_HUMAN68K)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "m68k" AND CMAKE_SYSTEM_NAME STREQUAL "Generic")
    # Check if Human68k headers are available
    set(CMAKE_REQUIRED_QUIET TRUE)
    check_include_file("x68k/dos.h" HAVE_X68K_DOS_H)
    if(HAVE_X68K_DOS_H)
      set(TERSE_PLATFORM_HUMAN68K ON)
      message(STATUS "Auto-detected Human68k platform")
    endif()
  endif()
endif()

if(TERSE_PLATFORM_HUMAN68K)
  target_sources(terse PRIVATE src/terse_human68k.c)
elseif(WIN32)
  target_sources(terse PRIVATE src/terse_windows.c)
  # Ensure Unicode (UTF-16) Windows API is used
  target_compile_definitions(terse PRIVATE UNICODE _UNICODE)
elseif(UNIX)
  target_sources(terse PRIVATE src/terse_posix.c)
else()
  target_sources(terse PRIVATE src/terse_platform_stub.c)
endif()
check_include_file(poll.h HAVE_POLL_H)
if(HAVE_POLL_H)
  target_compile_definitions(terse PRIVATE TERSE_HAVE_POLL_H=1)
endif()

target_include_directories(terse
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(TERSE_USE_SYSTEM_ICONV)
  message(STATUS "Using system iconv for codec conversions")
  find_package(Iconv REQUIRED)
  target_link_libraries(terse PRIVATE Iconv::Iconv)
  target_compile_definitions(terse PUBLIC TERSE_USE_SYSTEM_ICONV=1)
else()
  message(STATUS "Using built-in mini_iconv for codec conversions")
  target_sources(terse PRIVATE src/mini_iconv.c)
  target_compile_definitions(terse PUBLIC TERSE_USE_SYSTEM_ICONV=0)
endif()
target_compile_definitions(terse PUBLIC TERSE_HAVE_ICONV=1)

if(TERSE_ENABLE_TEST_MODE)
  target_compile_definitions(terse PUBLIC TERSE_ENABLE_TEST_MODE=1)
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  install(TARGETS terse
    EXPORT terseTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  )

  install(DIRECTORY include/
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.h"
  )
endif()

if(TERSE_BUILD_TESTING)
  add_subdirectory(tests)
endif()

# ツールのデフォルト: トップレベルプロジェクトならON、サブプロジェクトならOFF
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  option(TERSE_BUILD_TOOLS "Build auxiliary tooling" ON)
else()
  option(TERSE_BUILD_TOOLS "Build auxiliary tooling" OFF)
endif()

if(TERSE_BUILD_TOOLS)
  add_subdirectory(tools)
endif()

# サンプルのデフォルト: トップレベルプロジェクトならON、サブプロジェクトならOFF
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  option(TERSE_BUILD_SAMPLES "Build sample programs" ON)
else()
  option(TERSE_BUILD_SAMPLES "Build sample programs" OFF)
endif()

if(TERSE_BUILD_SAMPLES)
  add_subdirectory(samples)
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  set(TERSE_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/terse")

  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/terseConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )

  configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/terseConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/terseConfig.cmake"
    INSTALL_DESTINATION "${TERSE_INSTALL_CMAKEDIR}"
  )

  install(EXPORT terseTargets
    FILE terseTargets.cmake
    NAMESPACE terse::
    DESTINATION "${TERSE_INSTALL_CMAKEDIR}"
  )

  install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/terseConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/terseConfigVersion.cmake"
    DESTINATION "${TERSE_INSTALL_CMAKEDIR}"
  )
endif()

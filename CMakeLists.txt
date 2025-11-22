cmake_minimum_required(VERSION 3.30)
project(terse VERSION 0.1.0 LANGUAGES C CXX)

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

if(TERSE_BUILD_TESTING)
  add_subdirectory(external/attest)
endif()

add_subdirectory(c)

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

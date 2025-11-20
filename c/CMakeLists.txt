add_library(terse
  src/terse.c
  src/terse_unicode.c
  src/terse_codec.c
  src/terse_detection.c
  src/terse_event_helpers.c
  src/terse_input.c
  src/terse_style.c
)

include(CheckIncludeFile)

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

if(TERSE_ENABLE_ICONV)
  find_package(Iconv REQUIRED)
  target_link_libraries(terse PRIVATE Iconv::Iconv)
  target_compile_definitions(terse PUBLIC TERSE_USE_SYSTEM_ICONV=1)
else()
  target_sources(terse PRIVATE src/mini_iconv.c)
  target_compile_definitions(terse PUBLIC TERSE_USE_SYSTEM_ICONV=0)
endif()
target_compile_definitions(terse PUBLIC TERSE_HAVE_ICONV=1)

if(TERSE_ENABLE_TEST_MODE)
  target_compile_definitions(terse PUBLIC TERSE_ENABLE_TEST_MODE=1)
endif()

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

if(TERSE_BUILD_TESTING)
  add_subdirectory(tests)
endif()

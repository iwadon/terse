add_library(terse
  src/terse.c
)

if(TERSE_PLATFORM_HUMAN68K)
  target_sources(terse PRIVATE src/terse_human68k.c)
elseif(UNIX)
  target_sources(terse PRIVATE src/terse_posix.c)
else()
  target_sources(terse PRIVATE src/terse_platform_stub.c)
endif()

include(CheckIncludeFile)
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

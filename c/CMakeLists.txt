add_library(terse
  src/terse.c
)

if(UNIX)
  target_sources(terse PRIVATE src/terse_posix.c)
else()
  target_sources(terse PRIVATE src/terse_platform_stub.c)
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

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

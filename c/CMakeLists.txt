add_library(terse
  src/terse.c
)

target_include_directories(terse
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(TERSE_ENABLE_ICONV)
  find_package(Iconv REQUIRED)
  target_link_libraries(terse PRIVATE Iconv::Iconv)
  target_compile_definitions(terse PUBLIC TERSE_HAVE_ICONV=1)
else()
  target_compile_definitions(terse PUBLIC TERSE_HAVE_ICONV=0)
endif()

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

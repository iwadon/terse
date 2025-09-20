add_library(terse
  src/terse.c
  src/terse_color.c
)

target_include_directories(terse
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

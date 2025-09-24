add_library(terse
  src/terse.c
)

find_package(Iconv REQUIRED)

target_include_directories(terse
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(terse
  PRIVATE
    Iconv::Iconv
)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

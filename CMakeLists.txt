cmake_minimum_required(VERSION 3.30)
project(terse C CXX)

include(CTest)
add_subdirectory(c)

option(TERSE_BUILD_TOOLS "Build auxiliary tooling" ON)
if(TERSE_BUILD_TOOLS)
  add_subdirectory(tools)
endif()

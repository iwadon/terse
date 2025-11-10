add_executable(terse_inspect_terminal
  inspect_terminal.c
)

# This tool relies only on standard C/POSIX headers; no extra libs needed.

add_executable(terse_profile_info
  profile_info.c
)

target_link_libraries(terse_profile_info PRIVATE terse)

# Sample programs for terse library

if(MSVC)
  add_compile_options(/source-charset:utf-8)
endif()

# List of all sample programs
if(TERSE_PLATFORM_HUMAN68K)
set(SAMPLES
	p0_demo
	p1_color_demo
	p1_style_demo
	event_logger_demo_human68k
)
else()
set(SAMPLES
	p0_demo
	p1_color_demo
	p1_style_demo
	p2_features_demo
	p3_notifications_demo
	p3_image_demo
	p3_sixel_demo
	p3_kitty_graphics_demo
	p3_image_protocol_fallback_demo
	event_logger_demo
	line_edit_demo
	mouse_click_demo
	input_complete_demo
	cursor_position_test
)
  endif()

# Create executable for each sample
foreach(SAMPLE ${SAMPLES})
	add_executable(${SAMPLE} ${SAMPLE}.c)
	target_link_libraries(${SAMPLE} PRIVATE terse)
	target_include_directories(${SAMPLE} PRIVATE ${CMAKE_SOURCE_DIR}/c/include)
	# Ensure Unicode (UTF-16) Windows API is used on Windows
	if(WIN32)
		target_compile_definitions(${SAMPLE} PRIVATE UNICODE _UNICODE)
	endif()
	# POSIX拡張API (sigaction, nanosleep等) を有効にする
	if(UNIX AND NOT TERSE_PLATFORM_HUMAN68K)
		target_compile_definitions(${SAMPLE} PRIVATE _POSIX_C_SOURCE=200809L _XOPEN_SOURCE=700)
	endif()
endforeach()

# Human68k-specific programs (only built when targeting Human68k)
if(TERSE_PLATFORM_HUMAN68K)
	add_executable(bench_human68k bench_human68k.c)
	target_link_libraries(bench_human68k PRIVATE terse)
	target_include_directories(bench_human68k PRIVATE ${CMAKE_SOURCE_DIR}/c/include)

	# Low-level cursor position debug tool (doesn't use terse)
	add_executable(cursor_position_debug cursor_position_debug.c)
	target_include_directories(cursor_position_debug PRIVATE ${CMAKE_SOURCE_DIR}/c/include)

	# Low-level keycode dump tool (doesn't use terse)
	add_executable(keycode_dump_human68k keycode_dump_human68k.c)
	target_include_directories(keycode_dump_human68k PRIVATE ${CMAKE_SOURCE_DIR}/c/include)
endif()

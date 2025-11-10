# Sample programs for terse library

# List of all sample programs
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
)

# Create executable for each sample
foreach(SAMPLE ${SAMPLES})
	add_executable(${SAMPLE} ${SAMPLE}.c)
	target_link_libraries(${SAMPLE} PRIVATE terse)
	target_include_directories(${SAMPLE} PRIVATE ${CMAKE_SOURCE_DIR}/c/include)
endforeach()

# Human68k-specific benchmark (only built when targeting Human68k)
if(TERSE_PLATFORM_HUMAN68K)
	add_executable(bench_human68k bench_human68k.c)
	target_link_libraries(bench_human68k PRIVATE terse)
	target_include_directories(bench_human68k PRIVATE ${CMAKE_SOURCE_DIR}/c/include)
endif()

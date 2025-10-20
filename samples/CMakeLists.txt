# Sample programs for terse library

# List of all sample programs
set(SAMPLES
	p0_demo
	p1_color_demo
	p1_style_demo
	p2_features_demo
	p3_notifications_demo
	p3_image_demo
	event_logger_demo
	line_edit_demo
)

# Create executable for each sample
foreach(SAMPLE ${SAMPLES})
	add_executable(${SAMPLE} ${SAMPLE}.c)
	target_link_libraries(${SAMPLE} PRIVATE terse)
	target_include_directories(${SAMPLE} PRIVATE ${CMAKE_SOURCE_DIR}/c/include)
endforeach()

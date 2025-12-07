#include "terse.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sample_compat.h"

// Generate a simple 16x16 RGBA test pattern (4-color gradient)
static unsigned char *generate_test_image(size_t *out_size)
{
	const int width = 16;
	const int height = 16;
	const int bytes_per_pixel = 4; // RGBA
	size_t image_size = width * height * bytes_per_pixel;

	unsigned char *data = (unsigned char *)malloc(image_size);
	if (!data) {
		return NULL;
	}

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int offset = (y * width + x) * bytes_per_pixel;
			// Four-color quadrant pattern
			if (x < width / 2 && y < height / 2) {
				// Top-left: Red
				data[offset + 0] = 255;
				data[offset + 1] = 0;
				data[offset + 2] = 0;
				data[offset + 3] = 255;
			} else if (x >= width / 2 && y < height / 2) {
				// Top-right: Green
				data[offset + 0] = 0;
				data[offset + 1] = 255;
				data[offset + 2] = 0;
				data[offset + 3] = 255;
			} else if (x < width / 2 && y >= height / 2) {
				// Bottom-left: Blue
				data[offset + 0] = 0;
				data[offset + 1] = 0;
				data[offset + 2] = 255;
				data[offset + 3] = 255;
			} else {
				// Bottom-right: Yellow
				data[offset + 0] = 255;
				data[offset + 1] = 255;
				data[offset + 2] = 0;
				data[offset + 3] = 255;
			}
		}
	}

	if (out_size) {
		*out_size = image_size;
	}
	return data;
}

static const char *image_format_name(terse_image_format_t format)
{
	switch (format) {
	case TERSE_IMAGE_FORMAT_AUTO:
		return "AUTO";
	case TERSE_IMAGE_FORMAT_PNG:
		return "PNG";
	case TERSE_IMAGE_FORMAT_JPEG:
		return "JPEG";
	case TERSE_IMAGE_FORMAT_SIXEL:
		return "Sixel";
	case TERSE_IMAGE_FORMAT_KITTY:
		return "Kitty";
	default:
		return "Unknown";
	}
}

int main(void)
{
	printf("Image Protocol Fallback & Auto-Selection Demo\n");
	printf("==============================================\n\n");

	// Open terse with P3 profile to enable all features
	terse_options_t options = {
		.input_fd = STDIN_FILENO,
		.output_fd = STDOUT_FILENO,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = TERSE_CAP_ENABLE_IMAGE_INLINE,
	};

	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, &options);
	if (!handle) {
		perror("terse_open");
		return EXIT_FAILURE;
	}

	// Display terminal capabilities
	terse_capabilities_t caps = terse_get_capabilities(handle);
	printf("Terminal Capabilities:\n");
	printf("----------------------\n");
	printf("Profile: P%d\n", caps.profile);
	printf("Image support: ");
	switch (caps.images) {
	case TERSE_IMAGE_NONE:
		printf("None\n");
		break;
	case TERSE_IMAGE_ITERM_INLINE:
		printf("iTerm2 inline images\n");
		break;
	case TERSE_IMAGE_SIXEL:
		printf("Sixel graphics\n");
		break;
	case TERSE_IMAGE_KITTY:
		printf("Kitty graphics protocol\n");
		break;
	default:
		printf("Unknown (%d)\n", caps.images);
		break;
	}
	printf("Mouse support: %s\n", caps.mouse ? "Yes" : "No");
	printf("Clipboard write: %s\n", caps.has_clipboard_write ? "Yes" : "No");
	printf("\n");

	// Generate test pattern
	size_t image_size = 0;
	unsigned char *image_data = generate_test_image(&image_size);
	if (!image_data) {
		fprintf(stderr, "Failed to generate test image\n");
		terse_close(handle);
		return EXIT_FAILURE;
	}

	printf("Testing protocol priority and fallback behavior...\n");
	printf("Expected priority: Kitty > iTerm2 > Sixel > None\n\n");

	// Test each protocol individually
	terse_image_format_t formats[] = {
		TERSE_IMAGE_FORMAT_KITTY,
		TERSE_IMAGE_FORMAT_SIXEL,
	};

	for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); i++) {
		printf("------------------------------------------------------------\n");
		printf("Test %zu: Trying %s protocol (strict mode, no degradation)\n", i + 1, image_format_name(formats[i]));
		printf("------------------------------------------------------------\n");

		terse_image_request_t request = {
			.data = image_data,
			.size = image_size,
			.name = "test-pattern-16x16",
			.format = formats[i],
			.width = 16,
			.height = 16,
			.flags = TERSE_IMAGE_FLAG_INLINE,
		};

		int rc = terse_display_image(handle, &request);
		if (rc < 0) {
			fprintf(stderr, "Result: FAILED (%s)\n", strerror(-rc));
			terse_error_t err = terse_get_last_error(handle);
			fprintf(stderr, "Error: %d\n", err);
			if (rc == -ENOTSUP) {
				printf("(This protocol is not supported by your terminal)\n");
			}
		} else {
			printf("\n[Image displayed above using %s protocol]\n", image_format_name(formats[i]));
			printf("Result: SUCCESS\n");
		}
		printf("\n");
	}

	// Test automatic protocol selection
	printf("------------------------------------------------------------\n");
	printf("Test 4: Auto-detection (TERSE_IMAGE_FORMAT_AUTO)\n");
	printf("------------------------------------------------------------\n");
	printf("This will automatically select the best available protocol.\n\n");

	terse_image_request_t request_auto = {
		.data = image_data,
		.size = image_size,
		.name = "test-pattern-16x16",
		.format = TERSE_IMAGE_FORMAT_AUTO,
		.width = 16,
		.height = 16,
		.flags = TERSE_IMAGE_FLAG_INLINE | TERSE_IMAGE_FLAG_ALLOW_DEGRADE,
	};

	int rc = terse_display_image(handle, &request_auto);
	if (rc < 0) {
		fprintf(stderr, "Result: FAILED (%s)\n", strerror(-rc));
	} else {
		printf("\n[Image displayed above using auto-detected protocol]\n");
		printf("Result: SUCCESS\n");
		printf("(Check your terminal to see which protocol was used)\n");
	}
	printf("\n");

	// Test degradation with multiple protocols
	printf("------------------------------------------------------------\n");
	printf("Test 5: Fallback behavior with ALLOW_DEGRADE\n");
	printf("------------------------------------------------------------\n");
	printf("Request Kitty, but allow degradation to iTerm2 or Sixel.\n\n");

	terse_image_request_t request_degrade = {
		.data = image_data,
		.size = image_size,
		.name = "test-pattern-16x16",
		.format = TERSE_IMAGE_FORMAT_KITTY,
		.width = 16,
		.height = 16,
		.flags = TERSE_IMAGE_FLAG_INLINE | TERSE_IMAGE_FLAG_ALLOW_DEGRADE,
	};

	rc = terse_display_image(handle, &request_degrade);
	if (rc < 0) {
		fprintf(stderr, "Result: FAILED (%s)\n", strerror(-rc));
		printf("(Even with degradation, no suitable protocol was found)\n");
	} else {
		printf("\n[Image displayed above with degradation allowed]\n");
		printf("Result: SUCCESS\n");
		if (caps.images == TERSE_IMAGE_KITTY) {
			printf("(Likely used Kitty protocol)\n");
		} else if (caps.images == TERSE_IMAGE_ITERM_INLINE) {
			printf("(Likely degraded to iTerm2 protocol)\n");
		} else if (caps.images == TERSE_IMAGE_SIXEL) {
			printf("(Likely degraded to Sixel protocol)\n");
		} else {
			printf("(Unknown protocol used)\n");
		}
	}
	printf("\n");

	free(image_data);

	// Display summary
	printf("============================================================\n");
	printf("Summary\n");
	printf("============================================================\n");
	printf("Protocol priority (from highest to lowest):\n");
	printf("1. Kitty graphics protocol (most advanced)\n");
	printf("2. iTerm2 inline images (widely supported)\n");
	printf("3. Sixel graphics (legacy but compatible)\n");
	printf("4. None (unsupported)\n\n");
	printf("Flags:\n");
	printf("- TERSE_IMAGE_FLAG_ALLOW_DEGRADE: Allow graceful fallback\n");
	printf("- Without flag: Strict protocol requirement (may fail)\n\n");
	printf("Format options:\n");
	printf("- TERSE_IMAGE_FORMAT_AUTO: Automatically select best protocol\n");
	printf("- TERSE_IMAGE_FORMAT_KITTY/ITERM2/SIXEL: Request specific protocol\n");
	printf("\n");
	printf("Press Enter to exit.\n");
	(void)getchar();

	terse_close(handle);
	return EXIT_SUCCESS;
}

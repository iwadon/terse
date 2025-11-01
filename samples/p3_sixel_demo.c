#include "terse.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Generate a 256x128 RGB test pattern with horizontal gradient
static unsigned char *generate_test_image(size_t *out_size)
{
	const int width = 256;
	const int height = 128;
	const int bytes_per_pixel = 3;
	size_t image_size = width * height * bytes_per_pixel;

	unsigned char *data = (unsigned char *)malloc(image_size);
	if (!data) {
		return NULL;
	}

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int offset = (y * width + x) * bytes_per_pixel;
			// Horizontal gradient (red -> green -> blue)
			data[offset + 0] = (unsigned char)((x * 255) / width);  // R
			data[offset + 1] = (unsigned char)((y * 255) / height); // G
			data[offset + 2] = (unsigned char)(((width - x) * 255) / width); // B
		}
	}

	if (out_size) {
		*out_size = image_size;
	}
	return data;
}

int main(void)
{
	printf("Sixel Graphics Protocol Demo\n");
	printf("=============================\n\n");

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

	// Check capabilities
	terse_capabilities_t caps = terse_get_capabilities(handle);
	printf("Detected profile: P%d\n", caps.profile);
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
		printf("Unknown\n");
		break;
	}
	printf("\n");

	if (caps.images != TERSE_IMAGE_SIXEL) {
		printf("This demo requires Sixel graphics support.\n");
		printf("Try running in xterm, mlterm, or terminal with Sixel support.\n");
		printf("Or the image will be degraded/skipped.\n\n");
	}

	// Generate test pattern
	size_t image_size = 0;
	unsigned char *image_data = generate_test_image(&image_size);
	if (!image_data) {
		fprintf(stderr, "Failed to generate test image\n");
		terse_close(handle);
		return EXIT_FAILURE;
	}

	// Test 1: Display with Sixel format only (strict)
	printf("Test 1: Display with TERSE_IMAGE_FORMAT_SIXEL\n");
	printf("(Sixel-only mode, will fail if unsupported)\n\n");

	terse_image_request_t request1 = {
		.data = image_data,
		.size = image_size,
		.name = "test-gradient-256x128",
		.format = TERSE_IMAGE_FORMAT_SIXEL,
		.width = 256,
		.height = 128,
		.flags = TERSE_IMAGE_FLAG_INLINE,
	};

	int rc = terse_display_image(handle, &request1);
	if (rc < 0) {
		fprintf(stderr, "terse_display_image failed: %s\n", strerror(-rc));
		terse_error_info_t err = terse_get_last_error(handle);
		fprintf(stderr, "Error category: %d, code: %d\n", err.category, err.code);
	} else {
		printf("\n[Image displayed above using Sixel protocol]\n");
	}
	printf("\n");

	// Test 2: Display with ALLOW_DEGRADE flag (graceful fallback)
	printf("Test 2: Display with TERSE_IMAGE_FLAG_ALLOW_DEGRADE\n");
	printf("(This will try Sixel, but allow graceful fallback)\n\n");

	terse_image_request_t request2 = {
		.data = image_data,
		.size = image_size,
		.name = "test-gradient-256x128",
		.format = TERSE_IMAGE_FORMAT_SIXEL,
		.width = 256,
		.height = 128,
		.flags = TERSE_IMAGE_FLAG_INLINE | TERSE_IMAGE_FLAG_ALLOW_DEGRADE,
	};

	rc = terse_display_image(handle, &request2);
	if (rc < 0) {
		fprintf(stderr, "terse_display_image failed: %s\n", strerror(-rc));
	} else {
		printf("\n[Image displayed above with degradation allowed]\n");
	}
	printf("\n");

	// Test 3: Try AUTO format to test protocol priority
	printf("Test 3: Auto-format selection (should select best available protocol)\n");
	printf("(This tests the protocol priority: kitty > iTerm2 > Sixel)\n\n");

	terse_image_request_t request3 = {
		.data = image_data,
		.size = image_size,
		.name = "test-gradient-256x128",
		.format = TERSE_IMAGE_FORMAT_AUTO,
		.width = 256,
		.height = 128,
		.flags = TERSE_IMAGE_FLAG_INLINE | TERSE_IMAGE_FLAG_ALLOW_DEGRADE,
	};

	rc = terse_display_image(handle, &request3);
	if (rc < 0) {
		fprintf(stderr, "terse_display_image failed: %s\n", strerror(-rc));
	} else {
		printf("\n[Image displayed above using auto-selected protocol]\n");
	}
	printf("\n");

	free(image_data);

	// Display summary
	printf("Summary:\n");
	printf("--------\n");
	printf("Sixel graphics protocol features:\n");
	printf("- Widely supported in many terminal emulators\n");
	printf("- Efficient for simple graphics and color palettes\n");
	printf("- Standardized VT340 extension\n");
	printf("- Graceful degradation available with ALLOW_DEGRADE flag\n");
	printf("\n");
	printf("Press Enter to exit.\n");
	(void)getchar();

	terse_close(handle);
	return EXIT_SUCCESS;
}

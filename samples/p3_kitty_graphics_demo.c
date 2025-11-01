#include "terse.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Generate a 64x64 RGB test pattern with horizontal gradient
static unsigned char *generate_test_image(size_t *out_size)
{
	const int width = 64;
	const int height = 64;
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

// Convert raw RGB to simple PNG format
// This is a minimal PNG encoder for demonstration purposes
static unsigned char *rgb_to_png(const unsigned char *rgb_data, int width, int height, size_t *out_size)
{
	// For simplicity, return raw RGB data and let the terminal handle it
	// In production, you'd use a proper PNG encoder
	// Kitty graphics protocol supports raw RGB format (f=24)
	*out_size = width * height * 3;
	unsigned char *result = (unsigned char *)malloc(*out_size);
	if (result) {
		memcpy(result, rgb_data, *out_size);
	}
	return result;
}

int main(void)
{
	printf("Kitty Graphics Protocol Demo\n");
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

	if (caps.images != TERSE_IMAGE_KITTY) {
		printf("This demo requires kitty graphics protocol support.\n");
		printf("Try running in kitty terminal, or the image will be degraded/skipped.\n\n");
	}

	// Generate test pattern
	size_t rgb_size = 0;
	unsigned char *rgb_data = generate_test_image(&rgb_size);
	if (!rgb_data) {
		fprintf(stderr, "Failed to generate test image\n");
		terse_close(handle);
		return EXIT_FAILURE;
	}

	// Convert to PNG (or in this case, keep as raw RGB for kitty f=24)
	size_t image_size = 0;
	unsigned char *image_data = rgb_to_png(rgb_data, 64, 64, &image_size);
	free(rgb_data);

	if (!image_data) {
		fprintf(stderr, "Failed to convert image data\n");
		terse_close(handle);
		return EXIT_FAILURE;
	}

	// Test 1: Display with ALLOW_DEGRADE flag (graceful fallback)
	printf("Test 1: Display with TERSE_IMAGE_FLAG_ALLOW_DEGRADE\n");
	printf("(This should display the image or silently skip if unsupported)\n\n");

	terse_image_request_t request1 = {
		.data = image_data,
		.size = image_size,
		.name = "test-gradient-64x64",
		.format = TERSE_IMAGE_FORMAT_KITTY,
		.width = 64,
		.height = 64,
		.flags = TERSE_IMAGE_FLAG_INLINE | TERSE_IMAGE_FLAG_ALLOW_DEGRADE,
	};

	int rc = terse_display_image(handle, &request1);
	if (rc < 0) {
		fprintf(stderr, "terse_display_image failed: %s\n", strerror(-rc));
	} else {
		printf("\n[Image displayed above]\n");
	}
	printf("\n");

	// Test 2: Display without ALLOW_DEGRADE flag (strict requirement)
	printf("Test 2: Display without TERSE_IMAGE_FLAG_ALLOW_DEGRADE\n");
	printf("(This will fail with ENOTSUP if kitty graphics is not available)\n\n");

	terse_image_request_t request2 = {
		.data = image_data,
		.size = image_size,
		.name = "test-gradient-64x64",
		.format = TERSE_IMAGE_FORMAT_KITTY,
		.width = 64,
		.height = 64,
		.flags = TERSE_IMAGE_FLAG_INLINE,
	};

	rc = terse_display_image(handle, &request2);
	if (rc < 0) {
		fprintf(stderr, "terse_display_image failed: %s (expected if not kitty)\n", strerror(-rc));
	} else {
		printf("\n[Image displayed above]\n");
	}
	printf("\n");

	// Test 3: Auto-format detection (let terse choose best protocol)
	printf("Test 3: Auto-format detection (TERSE_IMAGE_FORMAT_AUTO)\n");
	printf("(This will use the best available protocol: kitty, sixel, or iTerm2)\n\n");

	terse_image_request_t request3 = {
		.data = image_data,
		.size = image_size,
		.name = "test-gradient-64x64",
		.format = TERSE_IMAGE_FORMAT_AUTO,
		.width = 64,
		.height = 64,
		.flags = TERSE_IMAGE_FLAG_INLINE | TERSE_IMAGE_FLAG_ALLOW_DEGRADE,
	};

	rc = terse_display_image(handle, &request3);
	if (rc < 0) {
		fprintf(stderr, "terse_display_image failed: %s\n", strerror(-rc));
	} else {
		printf("\n[Image displayed above using auto-detected protocol]\n");
	}
	printf("\n");

	free(image_data);

	// Display summary
	printf("Summary:\n");
	printf("--------\n");
	printf("Kitty graphics protocol features:\n");
	printf("- High resolution image display\n");
	printf("- Efficient transfer using base64 encoding\n");
	printf("- Supports multiple image formats (PNG, JPEG, raw RGB)\n");
	printf("- Terminal-native rendering (no external dependencies)\n");
	printf("\n");
	printf("Press Enter to exit.\n");
	(void)getchar();

	terse_close(handle);
	return EXIT_SUCCESS;
}

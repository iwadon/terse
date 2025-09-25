#include "terse.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *filename_component(const char *path)
{
	const char *last_slash = strrchr(path, '/');
	if (!last_slash) {
		return path;
	}
	return last_slash + 1;
}

static unsigned char *read_file(const char *path, size_t *out_size)
{
	FILE *fp = fopen(path, "rb");
	if (!fp) {
		perror("fopen");
		return NULL;
	}
	if (fseek(fp, 0, SEEK_END) != 0) {
		perror("fseek");
		fclose(fp);
		return NULL;
	}
	long length = ftell(fp);
	if (length < 0) {
		perror("ftell");
		fclose(fp);
		return NULL;
	}
	if (length == 0) {
		fprintf(stderr, "Input file is empty.\n");
		fclose(fp);
		return NULL;
	}
	if (fseek(fp, 0, SEEK_SET) != 0) {
		perror("fseek");
		fclose(fp);
		return NULL;
	}
	unsigned char *data = (unsigned char *)malloc((size_t)length);
	if (!data) {
		perror("malloc");
		fclose(fp);
		return NULL;
	}
	size_t read_bytes = fread(data, 1, (size_t)length, fp);
	if (read_bytes != (size_t)length) {
		perror("fread");
		free(data);
		fclose(fp);
		return NULL;
	}
	fclose(fp);
	if (out_size) {
		*out_size = (size_t)length;
	}
	return data;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <image-file>\n", argv[0]);
		return EXIT_FAILURE;
	}

	size_t image_size = 0;
	unsigned char *image_data = read_file(argv[1], &image_size);
	if (!image_data) {
		return EXIT_FAILURE;
	}

	const char *display_name = filename_component(argv[1]);
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
		free(image_data);
		return EXIT_FAILURE;
	}

	terse_capabilities_t caps = terse_get_capabilities(handle);
	if (caps.images != TERSE_IMAGE_ITERM_INLINE) {
		fprintf(stderr, "This terminal does not advertise inline image support.\n");
		fprintf(stderr, "Try running inside iTerm2 or WezTerm, or enable the capability manually.\n");
		terse_close(handle);
		free(image_data);
		return EXIT_FAILURE;
	}

	printf("Rendering %s (%zu bytes) via OSC 1337 inline image...\n\n", display_name, image_size);
	fflush(stdout);

	int rc = terse_display_image_inline(handle, image_data, image_size, display_name);
	free(image_data);
	if (rc < 0) {
		fprintf(stderr, "terse_display_image_inline failed: %s\n", strerror(-rc));
		terse_close(handle);
		return EXIT_FAILURE;
	}

	int text_rc = terse_write_text(handle, "\nDone. Press Enter to exit.\n");
	if (text_rc < 0) {
		fprintf(stderr, "terse_write_text failed: %s\n", strerror(-text_rc));
	}
	fflush(stdout);
	(void)getchar();

	terse_close(handle);
	return EXIT_SUCCESS;
}

#include "terse.h"
#include "terse_handle.h"
#include "terse_graphics.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* External helper functions from terse.c */
extern int write_literal(terse_handle_t handle, const char *sequence);
extern int write_sequence(terse_handle_t handle, const char *data, size_t length);
extern void set_error(terse_handle_t handle, terse_error_t error);
extern void clear_error(terse_handle_t handle);
extern char *base64_encode(const unsigned char *data, size_t size, size_t *out_len);

/* ========================================================================
 * Payload validation helper (shared with terse_output.c)
 * ======================================================================== */

int
payload_has_disallowed_chars(const char *payload)
{
	if (!payload) {
		return 0;
	}
	for (const unsigned char *p = (const unsigned char *)payload; *p; ++p) {
		if (*p == 0x07 || *p == 0x1b) {
			return 1;
		}
	}
	return 0;
}

/* ========================================================================
 * Clipboard
 * ======================================================================== */

terse_error_t terse_set_clipboard(terse_handle_t handle, const char *data)
{
	TERSE_CHECK_HANDLE(handle);
	if (!data) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	if (!handle->capabilities.has_clipboard_write || !handle->capabilities.has_basic_output) {
		clear_error(handle);
		return 0;
	}
	if (payload_has_disallowed_chars(data)) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	size_t encoded_len = 0;
	char *encoded = base64_encode((const unsigned char *)data, strlen(data), &encoded_len);
	if (!encoded) {
		errno = ENOMEM;
		set_error(handle, TERSE_ERR_OUT_OF_MEMORY);
		return TERSE_ERR_OUT_OF_MEMORY;
	}
	if (write_literal(handle, "\x1b]52;;") != 0) {
		free(encoded);
		return handle->last_error;
	}
	if (write_sequence(handle, encoded, encoded_len) != 0) {
		free(encoded);
		return handle->last_error;
	}
	free(encoded);
	if (write_literal(handle, "\x07") != 0) {
		return handle->last_error;
	}
	clear_error(handle);
	return 0;
}

/* ========================================================================
 * Image display helpers
 * ======================================================================== */

int
send_iterm_inline_image(terse_handle_t handle, const unsigned char *data, size_t size, const char *name)
{
	if (payload_has_disallowed_chars(name)) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	size_t name_len = 0;
	char *name_encoded = base64_encode((const unsigned char *)name, strlen(name), &name_len);
	size_t data_len = 0;
	char *data_encoded = base64_encode(data, size, &data_len);
	if (!name_encoded || !data_encoded) {
		free(name_encoded);
		free(data_encoded);
		errno = ENOMEM;
		set_error(handle, TERSE_ERR_OUT_OF_MEMORY);
		return TERSE_ERR_OUT_OF_MEMORY;
	}
	char header[TERSE_TEXT_BUFFER_SIZE];
	int header_len = snprintf(header,
		sizeof(header),
		"\x1b]1337;File=name=%s;size=%zu;inline=1:",
		name_encoded,
		size);
	free(name_encoded);
	if (header_len <= 0 || (size_t)header_len >= sizeof(header)) {
		free(data_encoded);
		errno = EOVERFLOW;
		set_error(handle, TERSE_ERR_OVERFLOW);
		return TERSE_ERR_OVERFLOW;
	}
	if (write_sequence(handle, header, (size_t)header_len) != 0) {
		free(data_encoded);
		return handle->last_error;
	}
	if (write_sequence(handle, data_encoded, data_len) != 0) {
		free(data_encoded);
		return handle->last_error;
	}
	free(data_encoded);
	if (write_literal(handle, "\x07") != 0) {
		return handle->last_error;
	}
	clear_error(handle);
	return 0;
}

int
send_sixel_image(terse_handle_t handle, const unsigned char *data, size_t size, const char *name)
{
	(void)name;
	static const char prefix[] = "\x1bPq";
	static const char suffix[] = "\x1b\\";
	if (write_sequence(handle, prefix, sizeof(prefix) - 1) != 0) {
		return handle->last_error;
	}
	const size_t chunk_size = 1024;
	size_t offset = 0;
	while (offset < size) {
		size_t remaining = size - offset;
		size_t to_write = remaining > chunk_size ? chunk_size : remaining;
		if (write_sequence(handle, (const char *)data + offset, to_write) != 0) {
			return handle->last_error;
		}
		offset += to_write;
	}
	if (write_sequence(handle, suffix, sizeof(suffix) - 1) != 0) {
		return handle->last_error;
	}
	clear_error(handle);
	return 0;
}

int
send_kitty_image(terse_handle_t handle, const unsigned char *data, size_t size, const char *name)
{
	(void)name;
	size_t encoded_len = 0;
	char *encoded = base64_encode(data, size, &encoded_len);
	if (!encoded) {
		errno = ENOMEM;
		set_error(handle, TERSE_ERR_OUT_OF_MEMORY);
		return TERSE_ERR_OUT_OF_MEMORY;
	}
	const char prefix[] = "\x1b_Ga=T,f=100,m=1;";
	if (write_sequence(handle, prefix, sizeof(prefix) - 1) != 0) {
		free(encoded);
		return handle->last_error;
	}
	if (write_sequence(handle, encoded, encoded_len) != 0) {
		free(encoded);
		return handle->last_error;
	}
	free(encoded);
	const char suffix[] = "\x1b\\";
	if (write_sequence(handle, suffix, sizeof(suffix) - 1) != 0) {
		return handle->last_error;
	}
	clear_error(handle);
	return 0;
}

/* ========================================================================
 * Image display API
 * ======================================================================== */

terse_error_t terse_display_image_inline(terse_handle_t handle, const unsigned char *data, size_t size, const char *name)
{
	terse_image_request_t request = {
		.data = data,
		.size = size,
		.name = name,
		.format = TERSE_IMAGE_FORMAT_AUTO,
		.width = 0,
		.height = 0,
		.flags = TERSE_IMAGE_FLAG_INLINE | TERSE_IMAGE_FLAG_ALLOW_DEGRADE,
	};
	return terse_display_image(handle, &request);
}

terse_error_t terse_display_image(terse_handle_t handle, const terse_image_request_t *request)
{
	TERSE_CHECK_HANDLE(handle);
	if (!request) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	if (!request->data || request->size == 0) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	unsigned int flags = request->flags;
	if (flags == 0) {
		flags = TERSE_IMAGE_FLAG_ALLOW_DEGRADE | TERSE_IMAGE_FLAG_INLINE;
	}
	int degrade_allowed = (flags & TERSE_IMAGE_FLAG_ALLOW_DEGRADE) != 0;
	if (!handle->capabilities.has_basic_output) {
		if (degrade_allowed) {
			clear_error(handle);
			return 0;
		}
		errno = ENOTSUP;
		set_error(handle, TERSE_ERR_UNSUPPORTED);
		return TERSE_ERR_UNSUPPORTED;
	}
	terse_image_support_t available = handle->capabilities.images;
	if (available == TERSE_IMAGE_NONE) {
		if (degrade_allowed) {
			clear_error(handle);
			return 0;
		}
		errno = ENOTSUP;
		set_error(handle, TERSE_ERR_UNSUPPORTED);
		return TERSE_ERR_UNSUPPORTED;
	}
	terse_image_support_t target = available;
	switch (request->format) {
	case TERSE_IMAGE_FORMAT_AUTO:
	case TERSE_IMAGE_FORMAT_PNG:
	case TERSE_IMAGE_FORMAT_JPEG:
		break;
	case TERSE_IMAGE_FORMAT_SIXEL:
		if (available == TERSE_IMAGE_SIXEL) {
			target = TERSE_IMAGE_SIXEL;
			break;
		}
		if (degrade_allowed) {
			clear_error(handle);
			return 0;
		}
		errno = ENOTSUP;
		set_error(handle, TERSE_ERR_UNSUPPORTED);
		return TERSE_ERR_UNSUPPORTED;
	case TERSE_IMAGE_FORMAT_KITTY:
		if (available == TERSE_IMAGE_KITTY) {
			target = TERSE_IMAGE_KITTY;
			break;
		}
		if (degrade_allowed) {
			clear_error(handle);
			return 0;
		}
		errno = ENOTSUP;
		set_error(handle, TERSE_ERR_UNSUPPORTED);
		return TERSE_ERR_UNSUPPORTED;
	default:
		errno = ENOTSUP;
		set_error(handle, TERSE_ERR_UNSUPPORTED);
		return TERSE_ERR_UNSUPPORTED;
	}
	const char *name = request->name;
	if (!name || !*name) {
		name = "image";
	}
	(void)request->width;
	(void)request->height;
	switch (target) {
	case TERSE_IMAGE_ITERM_INLINE:
		return send_iterm_inline_image(handle, request->data, request->size, name);
	case TERSE_IMAGE_SIXEL:
		return send_sixel_image(handle, request->data, request->size, name);
	case TERSE_IMAGE_KITTY:
		return send_kitty_image(handle, request->data, request->size, name);
	case TERSE_IMAGE_NONE:
	default:
		if (degrade_allowed) {
			clear_error(handle);
			return 0;
		}
		errno = ENOTSUP;
		set_error(handle, TERSE_ERR_UNSUPPORTED);
		return TERSE_ERR_UNSUPPORTED;
	}
}

/* ========================================================================
 * Notifications
 * ======================================================================== */

terse_error_t terse_notify(terse_handle_t handle, terse_notification_kind_t kind, const char *payload)
{
	TERSE_CHECK_HANDLE(handle);
	unsigned int required = 0;
	switch (kind) {
	case TERSE_NOTIFICATION_KIND_BELL:
		required = TERSE_NOTIFICATION_SUPPORT_BELL;
		break;
	case TERSE_NOTIFICATION_KIND_VISUAL:
		required = TERSE_NOTIFICATION_SUPPORT_VISUAL;
		break;
	case TERSE_NOTIFICATION_KIND_DESKTOP:
		required = TERSE_NOTIFICATION_SUPPORT_DESKTOP;
		break;
	default:
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	if (!handle->capabilities.has_basic_output || (handle->capabilities.notifications & required) == 0) {
		clear_error(handle);
		return 0;
	}
	if (kind == TERSE_NOTIFICATION_KIND_DESKTOP) {
		if (!payload || payload_has_disallowed_chars(payload)) {
			errno = EINVAL;
			set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
			return TERSE_ERR_INVALID_ARGUMENT;
		}
		if (write_literal(handle, "\x1b]9;1;") != 0) {
			return handle->last_error;
		}
		if (write_sequence(handle, payload, strlen(payload)) != 0) {
			return handle->last_error;
		}
		if (write_literal(handle, "\x07") != 0) {
			return handle->last_error;
		}
		clear_error(handle);
		return 0;
	}
	if (kind == TERSE_NOTIFICATION_KIND_VISUAL) {
		if (write_literal(handle, "\x1b[?5h\x1b[?5l") != 0) {
			return handle->last_error;
		}
		clear_error(handle);
		return 0;
	}
	if (write_literal(handle, "\x07") != 0) {
		return handle->last_error;
	}
	clear_error(handle);
	return 0;
}

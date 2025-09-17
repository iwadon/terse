#include "terse.h"
#include <stddef.h>
#include <stdlib.h>

typedef struct terse_handle {
	terse_profile_t profile;
	terse_capabilities_t capabilities;
} *terse_handle_t;

terse_handle_t terse_open(terse_profile_t reqested_profile)
{
	terse_handle_t handle = malloc(sizeof(struct terse_handle));
	if (!handle) {
		return NULL;
	}

	handle->profile = reqested_profile;
	handle->capabilities.profile = reqested_profile; // 仮の実装
	return handle;
}

void terse_close(terse_handle_t handle)
{
	free(handle);
}

terse_capabilities_t terse_get_capabilities(terse_handle_t handle)
{
	if (!handle) {
		terse_capabilities_t empty = { .profile = TERSE_P0 };
		return empty;
	}
	return handle->capabilities;
}

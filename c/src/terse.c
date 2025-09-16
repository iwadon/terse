#include "terse.h"
#include <stddef.h>

terse_handle_t terse_open(terse_profile_t reqested_profile)
{
	(void)reqested_profile;
	return NULL;
}

void terse_close(terse_handle_t ctx)
{
	(void)ctx;
}

terse_capabilities_t terse_get_capabilities(terse_handle_t ctx)
{
	(void)ctx;
	terse_capabilities_t caps = { .profile = TERSE_P0 };
	return caps;
}

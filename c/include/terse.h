#ifndef TERSE_H_INCLUDED
#define TERSE_H_INCLUDED

typedef struct terse_handle *terse_handle_t;

typedef enum terse_profile {
	TERSE_P0 = 0,
	TERSE_P1,
	TERSE_P2,
	TERSE_P3
} terse_profile_t;

terse_handle_t terse_open(terse_profile_t reqested_profile);
void terse_close(terse_handle_t handle);

typedef struct terse_capabilities {
	terse_profile_t profile;
} terse_capabilities_t;

terse_capabilities_t terse_get_capabilities(terse_handle_t handle);

#endif // TERSE_H_INCLUDED

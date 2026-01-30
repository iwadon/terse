/*
 * test_compat.h - Cross-platform compatibility layer for tests
 *
 * Provides POSIX-compatible functions on Windows and defines macros
 * for conditionally skipping platform-specific tests.
 */
#ifndef TEST_COMPAT_H
#define TEST_COMPAT_H

#ifdef _WIN32

#include <fcntl.h>
#include <io.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

/* POSIX file descriptor constants */
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

/* Windows uses _close instead of close */
#define close _close

/* Windows does not have setenv/unsetenv, provide compatibility functions */
static inline int setenv(const char *name, const char *value, int overwrite)
{
	if (!overwrite) {
		size_t required_size;
		if (getenv_s(&required_size, NULL, 0, name) == 0 && required_size > 0) {
			return 0; /* Variable exists, don't overwrite */
		}
	}
	return _putenv_s(name, value);
}

static inline int unsetenv(const char *name)
{
	return _putenv_s(name, "");
}

/* ssize_t is not defined on Windows */
#ifndef _SSIZE_T_DEFINED
typedef intptr_t ssize_t;
#define _SSIZE_T_DEFINED
#endif

/* Define HAVE_PIPE to indicate pipe() availability */
/* Windows has _pipe() but it works differently from POSIX pipe() */
#undef HAVE_POSIX_PIPE

/* nanosleep compatibility */
static inline int nanosleep(const struct timespec *req, struct timespec *rem)
{
	(void)rem;
	DWORD ms = (DWORD)(req->tv_sec * 1000 + req->tv_nsec / 1000000);
	Sleep(ms);
	return 0;
}

#else /* POSIX */

#include <unistd.h>

/* POSIX systems have pipe() */
#define HAVE_POSIX_PIPE 1

#endif /* _WIN32 */

/*
 * SKIP_ON_WINDOWS - Use in test functions to skip tests that require
 * POSIX-specific features like pipe().
 *
 * Usage:
 *   TEST(Suite, TestName) {
 *       SKIP_ON_WINDOWS();
 *       // ... pipe-based test code ...
 *   }
 */
#ifdef _WIN32
#define SKIP_ON_WINDOWS() \
	do {                  \
		return;           \
	} while (0)
#else
#define SKIP_ON_WINDOWS() \
	do {                  \
	} while (0)
#endif

/*
 * SKIP_ON_POSIX - Use in test functions to skip tests that are
 * Windows-specific.
 */
#ifdef _WIN32
#define SKIP_ON_POSIX() \
	do {                \
	} while (0)
#else
#define SKIP_ON_POSIX() \
	do {                \
		return;         \
	} while (0)
#endif

#endif /* TEST_COMPAT_H */

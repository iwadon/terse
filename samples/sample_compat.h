/*
 * sample_compat.h - Cross-platform compatibility layer for samples
 *
 * Provides POSIX-compatible functions and constants on Windows.
 */
#ifndef SAMPLE_COMPAT_H
#define SAMPLE_COMPAT_H

#ifdef _WIN32

#include <io.h>
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

/* usleep compatibility (microseconds) */
static inline void usleep(unsigned int usec)
{
	/* Windows Sleep() takes milliseconds */
	Sleep((usec + 999) / 1000);
}

/* sleep compatibility (seconds) */
static inline unsigned int sleep(unsigned int seconds)
{
	Sleep(seconds * 1000);
	return 0;
}

#else /* POSIX */

#include <unistd.h>

#endif /* _WIN32 */

#endif /* SAMPLE_COMPAT_H */

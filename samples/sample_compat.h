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

/* sleep compatibility (seconds) */
static inline unsigned int sleep(unsigned int seconds)
{
	Sleep(seconds * 1000);
	return 0;
}

static inline void sample_sleep_ms(unsigned int msec)
{
	Sleep(msec);
}

#else /* POSIX */

#include <time.h>
#include <unistd.h>

/* nanosleep ベースのミリ秒スリープ
 * usleep は POSIX.1-2008 で廃止されたため代わりに nanosleep を使う */
static inline void sample_sleep_ms(unsigned int msec)
{
	struct timespec ts;
	ts.tv_sec = (time_t)(msec / 1000);
	ts.tv_nsec = (long)((msec % 1000) * 1000000L);
	nanosleep(&ts, NULL);
}

#endif /* _WIN32 */

#endif /* SAMPLE_COMPAT_H */

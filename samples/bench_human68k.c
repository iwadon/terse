#include "terse.h"

#include <stdio.h>
#include <unistd.h>

/* Detect Human68k platform */
#if defined(__human68k__) || defined(__HUMAN68K__)
#define BENCH_HUMAN68K 1
#include <x68k/iocs.h>
#include <x68k/dos.h>
#else
#define BENCH_HUMAN68K 0
#include <time.h>
#endif

/* Benchmark parameters */
#define BENCH_ITERATIONS 1000
#define BENCH_SCREEN_ROWS 24
#define BENCH_SCREEN_COLS 80

/* Benchmark result storage */
struct bench_result {
	const char *name;
	int iterations;
	double elapsed_ms;
	double ops_per_sec;
};

static struct bench_result results[10];
static int result_count = 0;

static void
record_result(const char *name, int iterations, double elapsed_ms)
{
	if (result_count < 10) {
		results[result_count].name = name;
		results[result_count].iterations = iterations;
		results[result_count].elapsed_ms = elapsed_ms;
		results[result_count].ops_per_sec = (iterations / elapsed_ms) * 1000.0;
		result_count++;
	}
}

static double
get_time_ms(void)
{
#if BENCH_HUMAN68K
	/* Use Human68k IOCS _ONTIME for timing */
	struct iocs_time t = _iocs_ontime();
	/* sec is in centiseconds (1/100 sec), day is days since boot */
	return (t.day * 86400.0 + t.sec / 100.0) * 1000.0;
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
#endif
}

static void
benchmark_cursor_moves_terse(terse_handle_t handle)
{
	double start = get_time_ms();

	for (int i = 0; i < BENCH_ITERATIONS; i++) {
		int row = i % BENCH_SCREEN_ROWS;
		int col = i % BENCH_SCREEN_COLS;
		terse_move_to(handle, row, col);
	}

	double end = get_time_ms();
	double elapsed = end - start;
	record_result("Terse cursor moves", BENCH_ITERATIONS, elapsed);
}

#if BENCH_HUMAN68K
static void
benchmark_cursor_moves_direct(void)
{
	double start = get_time_ms();

	for (int i = 0; i < BENCH_ITERATIONS; i++) {
		int row = i % BENCH_SCREEN_ROWS;
		int col = i % BENCH_SCREEN_COLS;
		_iocs_b_locate(col, row);
	}

	double end = get_time_ms();
	double elapsed = end - start;
	record_result("Direct IOCS moves", BENCH_ITERATIONS, elapsed);
}
#endif

static void
benchmark_text_output_terse(terse_handle_t handle)
{
	const char *text = "Hello, Human68k!";
	double start = get_time_ms();

	for (int i = 0; i < BENCH_ITERATIONS; i++) {
		terse_move_to(handle, 0, 0);
		terse_write_text(handle, text);
	}

	double end = get_time_ms();
	double elapsed = end - start;
	record_result("Terse text output", BENCH_ITERATIONS, elapsed);
}

#if BENCH_HUMAN68K
static void
benchmark_text_output_direct(void)
{
	const char *text = "Hello, Human68k!";
	double start = get_time_ms();

	for (int i = 0; i < BENCH_ITERATIONS; i++) {
		_iocs_b_locate(0, 0);
		_iocs_b_print(text);
	}

	double end = get_time_ms();
	double elapsed = end - start;
	record_result("Direct IOCS output", BENCH_ITERATIONS, elapsed);
}
#endif

static void
benchmark_clear_screen_terse(terse_handle_t handle)
{
	double start = get_time_ms();

	for (int i = 0; i < BENCH_ITERATIONS / 10; i++) {
		terse_clear_screen(handle, TERSE_CLEAR_ALL);
	}

	double end = get_time_ms();
	double elapsed = end - start;
	record_result("Terse clear screen", BENCH_ITERATIONS / 10, elapsed);
}

#if BENCH_HUMAN68K
static void
benchmark_clear_screen_direct(void)
{
	double start = get_time_ms();

	for (int i = 0; i < BENCH_ITERATIONS / 10; i++) {
		/* Clear entire screen using IOCS _B_CLR_AL */
		_iocs_b_clr_al();
	}

	double end = get_time_ms();
	double elapsed = end - start;
	record_result("Direct IOCS clear", BENCH_ITERATIONS / 10, elapsed);
}
#endif

int
main(void)
{
	terse_options_t options = {
		.input_fd = STDIN_FILENO,
		.output_fd = STDOUT_FILENO,
		.codec_name = "Shift_JIS",
		.disabled_caps = 0,
		.enabled_caps = 0
	};

	terse_handle_t handle = terse_open(TERSE_P0, &options);
	if (!handle) {
		fprintf(stderr, "terse_open failed\n");
		return 1;
	}

	/* Clear screen before starting */
	terse_clear_screen(handle, TERSE_CLEAR_ALL);
	terse_move_to(handle, 0, 0);

	printf("Human68k Terminal Performance Benchmark\n");
	printf("Running tests... (screen will update during tests)\n\n");

	/* Run all benchmarks silently */
	benchmark_cursor_moves_terse(handle);
#if BENCH_HUMAN68K
	benchmark_cursor_moves_direct();
#endif

	benchmark_text_output_terse(handle);
#if BENCH_HUMAN68K
	benchmark_text_output_direct();
#endif

	benchmark_clear_screen_terse(handle);
#if BENCH_HUMAN68K
	benchmark_clear_screen_direct();
#endif

	/* Clear screen and show results */
	terse_clear_screen(handle, TERSE_CLEAR_ALL);
	terse_move_to(handle, 0, 0);

	printf("========================================\n");
	printf("Human68k Terminal Performance Results\n");
	printf("========================================\n\n");

	/* Print all results in a table format */
	printf("%-25s  %10s  %12s  %12s\n",
	       "Test Name", "Iterations", "Time (ms)", "Ops/sec");
	printf("%-25s  %10s  %12s  %12s\n",
	       "-------------------------", "----------", "------------", "------------");

	for (int i = 0; i < result_count; i++) {
		printf("%-25s  %10d  %12.2f  %12.0f\n",
		       results[i].name,
		       results[i].iterations,
		       results[i].elapsed_ms,
		       results[i].ops_per_sec);
	}

	printf("\n");

#if BENCH_HUMAN68K
	/* Calculate speedup ratios */
	if (result_count >= 6) {
		printf("Speedup (Direct IOCS vs Terse):\n");
		printf("  Cursor moves: %.1fx faster\n",
		       results[1].ops_per_sec / results[0].ops_per_sec);
		printf("  Text output:  %.1fx faster\n",
		       results[3].ops_per_sec / results[2].ops_per_sec);
		printf("  Clear screen: %.1fx faster\n",
		       results[5].ops_per_sec / results[4].ops_per_sec);
	}
#endif

	terse_close(handle);
	return 0;
}

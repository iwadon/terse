#ifndef TEST_H_INCLUDED
#define TEST_H_INCLUDED

#include <stdio.h>

typedef void (*TestFunc)(void);

typedef struct TestCase {
	const char *name;
	TestFunc func;
	struct TestCase *next;
} TestCase;

static TestCase *g_tests = NULL;
static int g_failed_tests = 0;
static int g_failures_total = 0;
static int g_failures_in_test = 0;

static void __test_register(const char *name, TestFunc func, TestCase *slot)
{
	slot->name = name;
	slot->func = func;
	slot->next = g_tests;
	g_tests = slot;
}

static void __test_fail_msg(const char *file, int line, const char *what)
{
	++g_failures_total;
	++g_failures_in_test;
	printf("  FAIL: %s:%d: %s\n", file, line, what);
}

#define TEST(SuiteName, TestName)                                                     \
	static void SuiteName##_##TestName(void);                                         \
	static void SuiteName##_##TestName##_register(void) __attribute__((constructor)); \
	static void SuiteName##_##TestName##_register(void)                               \
	{                                                                                 \
		static TestCase slot;                                                         \
		__test_register(#SuiteName "." #TestName, SuiteName##_##TestName, &slot);     \
	}                                                                                 \
	static void SuiteName##_##TestName(void)

#define EXPECT_TRUE(cond)                                                               \
	do {                                                                                \
		if (!(cond)) {                                                                  \
			__test_fail_msg(__FILE__, __LINE__, "expected true but got false: " #cond); \
		}                                                                               \
	} while (0)

#define EXPECT_EQ(expected, actual)                                                         \
	do {                                                                                    \
		if ((expected) != (actual)) {                                                       \
			__test_fail_msg(__FILE__, __LINE__, "expected " #expected " but got " #actual); \
		}                                                                                   \
	} while (0)

static inline int RunAllTests(void)
{
	int total_tests = 0;
	int failed_tests = 0;
	g_failed_tests = 0;
	g_failures_total = 0;
	for (TestCase *test = g_tests; test; test = test->next) {
		++total_tests;
		g_failures_in_test = 0;
		printf("[ RUN  ] %s\n", test->name);
		test->func();
		if (g_failures_in_test == 0) {
			printf("[ PASS ] %s\n", test->name);
		} else {
			printf("[ FAIL ] %s (%d failures)\n", test->name, g_failures_in_test);
			++failed_tests;
		}
	}
	printf("\nTotal tests: %d, Failed tests: %d, Total failures: %d\n", total_tests, failed_tests, g_failures_total);
	g_failed_tests = failed_tests;
	return g_failed_tests == 0 ? 0 : 1;
}

#endif // TEST_H_INCLUDED

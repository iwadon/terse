#include "terse.h"
#include "test.h"

TEST(TerseOpen, ReturnsNotNull_WithValidProfile)
{
	terse_handle_t handle = terse_open(TERSE_P1);
	EXPECT_TRUE(handle != NULL);
	terse_close(handle);
}

TEST(TerseOpen, ReturnsNull_WithInvalidProfile)
{
	terse_handle_t handle = terse_open((terse_profile_t)999);
	EXPECT_TRUE(handle == NULL);
}

int main()
{
	return RunAllTests();
}

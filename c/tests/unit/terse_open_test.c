#include "terse.h"
#include "test.h"

TEST(TerseOpen, ReturnsNonNull_OnValidProfile)
{
	terse_handle_t handle = terse_open(TERSE_P0, NULL);
	EXPECT_TRUE(handle != NULL);
	terse_close(handle);
}

TEST(TerseOpen, ReturnsNull_OnInvalidProfile)
{
	terse_handle_t handle = terse_open((terse_profile_t)999, NULL);
	EXPECT_TRUE(handle == NULL);
}

TEST(TerseOpen, ReturnsP0Profile_OnRequestAboveP0)
{
	terse_handle_t handle = terse_open(TERSE_P3, NULL);
	EXPECT_TRUE(handle != NULL);
	terse_capabilities_t caps = terse_get_capabilities(handle);
	EXPECT_EQ(caps.profile, TERSE_P0);
	terse_close(handle);
}

int main()
{
	return RunAllTests();
}

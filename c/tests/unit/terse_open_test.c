#include "terse.h"
#include "test.h"

TEST(TerseOpenTest, OpenAndClose)
{
	terse_handle_t handle = terse_open(TERSE_P1);
	EXPECT_TRUE(handle != NULL);
	terse_close(handle);
}

int main()
{
	return RunAllTests();
}

#include <stdio.h>
#include "test.h"

static int static_count;
struct static_struct
{
	int i;
	static_struct()
	{
		static_count++;
		i = 12;
	};
};

static static_struct ss;

int init_static(void)
{
	static static_struct s;
	return s.i;
}

void test_guards(void)
{
	init_static();
	int i = init_static();
	TEST(i == 12, "Static initialized");
	TEST(static_count == 2, "Each static only initialized once");
}

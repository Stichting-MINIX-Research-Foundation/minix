/*
getpagesize.c
*/

#include <unistd.h>

int getpagesize(void)
{
	/* We don't have paging. Pretend that we do. */
	return 4096;
}

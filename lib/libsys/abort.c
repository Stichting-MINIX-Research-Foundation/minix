#include <stdlib.h>
#include <minix/sysutil.h>

void abort()
{
	panic("Abort.");
}

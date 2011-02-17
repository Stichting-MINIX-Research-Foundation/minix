/*
getpagesize.c
*/
#include <sys/cdefs.h>
#include "namespace.h"

#include <machine/vmparam.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(getpagesize, _getpagesize)
#endif

int getpagesize(void)
{
	return PAGE_SIZE;
}

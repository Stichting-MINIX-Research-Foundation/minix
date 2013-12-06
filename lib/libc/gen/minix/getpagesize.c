/*
getpagesize.c
*/
#include <sys/cdefs.h>
#include <sys/types.h>
#include "namespace.h"

#include <machine/param.h>
#include <machine/vmparam.h>

#include <unistd.h>

#ifdef __weak_alias
__weak_alias(getpagesize, _getpagesize)
#endif

int getpagesize(void)
{
	return PAGE_SIZE;
}

#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>
#include <stdlib.h>
#include <unistd.h>

int closefrom(int fd)
{
	int f, ok = 0, e = 0;
	for(f = fd; f < OPEN_MAX; f++) {
		if(close(f) >= 0)
			ok = 1;
		else
			e = errno;
	}

	if(ok)
		return 0;

	/* all failed - return last valid error */
	errno = e;
	return -1;
}

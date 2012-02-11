#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <unistd.h>

int issetugid(void)
{
	int r;
	message m;

	r = _syscall(PM_PROC_NR, ISSETUGID, &m);
	if (r == -1) return 0;	/* Default to old behavior */
	return(r);
}

#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <unistd.h>

int issetugid(void)
{
	message m;

	memset(&m, 0, sizeof(m));
	return _syscall(PM_PROC_NR, PM_ISSETUGID, &m);
}

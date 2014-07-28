#include <sys/cdefs.h>
#include "namespace.h"

#include <errno.h>
#include <limits.h>
#include <sys/resource.h>
#include <unistd.h>

/* Simple stub for now. */
int setrlimit(int resource, const struct rlimit *rlp)
{
	rlim_t limit;
	
	switch (resource)
	{
		case RLIMIT_CPU:
		case RLIMIT_FSIZE:
		case RLIMIT_DATA:
		case RLIMIT_STACK:
		case RLIMIT_CORE:
		case RLIMIT_RSS:
		case RLIMIT_MEMLOCK:
		case RLIMIT_NPROC:
		case RLIMIT_NOFILE:
		case RLIMIT_SBSIZE:
		case RLIMIT_AS:
		/* case RLIMIT_VMEM: Same as RLIMIT_AS */
		case RLIMIT_NTHR:
			break;

		default:
			errno = EINVAL;
			return -1;
	}		

	return 0;
}


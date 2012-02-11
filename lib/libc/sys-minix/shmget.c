#define _SYSTEM	1
#define _MINIX 1
#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <minix/rs.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <errno.h>


#ifdef __weak_alias
__weak_alias(shmget, _shmget)
#endif

static int get_ipc_endpt(endpoint_t *pt)
{
	return minix_rs_lookup("ipc", pt);
}

/* Get shared memory segment. */
int shmget(key_t key, size_t size, int shmflg)
{
	message m;
	endpoint_t ipc_pt;
	int r;

	if (get_ipc_endpt(&ipc_pt) != OK) {
		errno = ENOSYS;
		return -1;
	}

	m.SHMGET_KEY = key;
	m.SHMGET_SIZE = size;
	m.SHMGET_FLAG = shmflg;

	r = _syscall(ipc_pt, IPC_SHMGET, &m);
	if (r != OK)
		return r;
	return m.SHMGET_RETID;
}

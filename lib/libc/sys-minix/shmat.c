#define _SYSTEM	1
#define _MINIX 1

#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <minix/rs.h>

#include <lib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <errno.h>

#ifdef __weak_alias
__weak_alias(shmat, _shmat)
__weak_alias(shmdt, _shmdt)
#endif


static int get_ipc_endpt(endpoint_t *pt)
{
	return minix_rs_lookup("ipc", pt);
}

/* Attach shared memory segment. */
void *shmat(int shmid, const void *shmaddr, int shmflg)
{
	message m;
	endpoint_t ipc_pt;
	int r;

	if (get_ipc_endpt(&ipc_pt) != OK) {
		errno = ENOSYS;
		return NULL;
	}

	m.SHMAT_ID = shmid;
	m.SHMAT_ADDR = (long) shmaddr;
	m.SHMAT_FLAG = shmflg;

	r = _syscall(ipc_pt, IPC_SHMAT, &m);
	if (r != OK)
		return (void *) -1;
	return (void *) m.SHMAT_RETADDR;
}

/* Deattach shared memory segment. */
int shmdt(const void *shmaddr)
{
	message m;
	endpoint_t ipc_pt;

	if (get_ipc_endpt(&ipc_pt) != OK) {
		errno = ENOSYS;
		return -1;
	}

	m.SHMDT_ADDR = (long) shmaddr;

	return _syscall(ipc_pt, IPC_SHMDT, &m);
}


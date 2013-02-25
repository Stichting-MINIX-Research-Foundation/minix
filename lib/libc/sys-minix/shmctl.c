#define _SYSTEM	1

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

static int get_ipc_endpt(endpoint_t *pt)
{
	return minix_rs_lookup("ipc", pt);
}

/* Shared memory control operation. */
int shmctl(int shmid, int cmd, struct shmid_ds *buf)
{
	message m;
	endpoint_t ipc_pt;
	int r;

	if (get_ipc_endpt(&ipc_pt) != OK) {
		errno = ENOSYS;
		return -1;
	}

	m.SHMCTL_ID = shmid;
	m.SHMCTL_CMD = cmd;
	m.SHMCTL_BUF = (long) buf;

	r = _syscall(ipc_pt, IPC_SHMCTL, &m);
	if ((cmd == IPC_INFO || cmd == SHM_INFO || cmd == SHM_STAT)
		&& (r == OK))
		return m.SHMCTL_RET;
	return r;
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(shmctl, __shmctl50)
#endif

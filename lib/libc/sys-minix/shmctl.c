#define _SYSTEM	1
#define _MINIX_SYSTEM	1

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
#include <string.h>

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

	memset(&m, 0, sizeof(m));
	m.m_lc_ipc_shmctl.id = shmid;
	m.m_lc_ipc_shmctl.cmd = cmd;
	m.m_lc_ipc_shmctl.buf = buf;

	r = _syscall(ipc_pt, IPC_SHMCTL, &m);
	if ((cmd == IPC_INFO || cmd == SHM_INFO || cmd == SHM_STAT)
		&& (r == OK))
		return m.m_lc_ipc_shmctl.ret;
	return r;
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(shmctl, __shmctl50)
#endif

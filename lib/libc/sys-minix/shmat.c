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

	memset(&m, 0, sizeof(m));
	m.m_lc_ipc_shmat.id = shmid;
	m.m_lc_ipc_shmat.addr = shmaddr;
	m.m_lc_ipc_shmat.flag = shmflg;

	r = _syscall(ipc_pt, IPC_SHMAT, &m);
	if (r != OK)
		return (void *) -1;
	return m.m_lc_ipc_shmat.retaddr;
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

	memset(&m, 0, sizeof(m));
	m.m_lc_ipc_shmdt.addr = shmaddr;

	return _syscall(ipc_pt, IPC_SHMDT, &m);
}


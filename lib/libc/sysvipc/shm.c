#define _SYSTEM	1
#define _MINIX 1

#include <minix/com.h>
#include <minix/config.h>
#include <minix/ipc.h>
#include <minix/endpoint.h>
#include <minix/sysutil.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/rs.h>

#include <lib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <errno.h>

PRIVATE int get_ipc_endpt(endpoint_t *pt)
{
	return minix_rs_lookup("ipc", pt);
}

/* Shared memory control operation. */
PUBLIC int shmctl(int shmid, int cmd, struct shmid_ds *buf)
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

/* Get shared memory segment. */
PUBLIC int shmget(key_t key, size_t size, int shmflg)
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

/* Attach shared memory segment. */
PUBLIC void *shmat(int shmid, const void *shmaddr, int shmflg)
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
PUBLIC int shmdt(const void *shmaddr)
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


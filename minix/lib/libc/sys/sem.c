#define __USE_MISC
#define _SYSTEM	1
#define _MINIX_SYSTEM	1

#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <lib.h>
#include <minix/rs.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

static int get_ipc_endpt(endpoint_t *pt)
{
	return minix_rs_lookup("ipc", pt);
}

/* Get semaphore.  */
int semget(key_t key, int nsems, int semflag)
{
	message m;
	endpoint_t ipc_pt;
	int r;

	if (get_ipc_endpt(&ipc_pt) != OK) {
		errno = ENOSYS;
		return -1;
	}

	memset(&m, 0, sizeof(m));
	m.m_lc_ipc_semget.key = key;
	m.m_lc_ipc_semget.nr = nsems;
	m.m_lc_ipc_semget.flag = semflag;

	r = _syscall(ipc_pt, IPC_SEMGET, &m);
	if (r != OK)
		return r;

	return m.m_lc_ipc_semget.retid;
}
 
/* Semaphore control operation.  */
int semctl(int semid, int semnum, int cmd, ...)
{
	message m;
	endpoint_t ipc_pt;
	va_list ap;
	int r;

	if (get_ipc_endpt(&ipc_pt) != OK) {
		errno = ENOSYS;
		return -1;
	}

	memset(&m, 0, sizeof(m));
	m.m_lc_ipc_semctl.id = semid;
	m.m_lc_ipc_semctl.num = semnum;
	m.m_lc_ipc_semctl.cmd = cmd;
	va_start(ap, cmd);
	if (cmd == IPC_STAT || cmd == IPC_SET || cmd == IPC_INFO ||
		cmd == SEM_INFO || cmd == SEM_STAT || cmd == GETALL ||
		cmd == SETALL || cmd == SETVAL)
		m.m_lc_ipc_semctl.opt = (long) va_arg(ap, long);
	va_end(ap); 

	r = _syscall(ipc_pt, IPC_SEMCTL, &m);
	if ((r != -1) && (cmd == GETNCNT || cmd == GETZCNT || cmd == GETPID ||
		cmd == GETVAL || cmd == IPC_INFO || cmd == SEM_INFO ||
		cmd == SEM_STAT))
		return m.m_lc_ipc_semctl.ret;
	return r;
}

/* Operate on semaphore.  */
int semop(int semid, struct sembuf *sops, size_t nsops)
{
	message m;
	endpoint_t ipc_pt;

	if (get_ipc_endpt(&ipc_pt) != OK) {
		errno = ENOSYS;
		return -1;
	}

	memset(&m, 0, sizeof(m));
	m.m_lc_ipc_semop.id = semid;
	m.m_lc_ipc_semop.ops = sops;
	m.m_lc_ipc_semop.size = nsops;

	return _syscall(ipc_pt, IPC_SEMOP, &m);
}


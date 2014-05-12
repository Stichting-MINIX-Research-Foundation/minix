#include "syslib.h"

#include <unistd.h>
#include <string.h>
#include <minix/safecopies.h>

int
checkperms(endpoint_t endpt, char *path, size_t size)
{
	cp_grant_id_t grant;
	message m;
	int r;

	if ((grant = cpf_grant_direct(VFS_PROC_NR, (vir_bytes) path, size,
	    CPF_READ | CPF_WRITE)) == GRANT_INVALID)
		return ENOMEM;

	memset(&m, 0, sizeof(m));
	m.m_lsys_vfs_checkperms.endpt = endpt;
	m.m_lsys_vfs_checkperms.grant = grant;
	m.m_lsys_vfs_checkperms.count = size;

	r = _taskcall(VFS_PROC_NR, VFS_CHECKPERMS, &m);

	cpf_revoke(grant);

	return r;
}

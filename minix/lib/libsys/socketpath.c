#include "syslib.h"

#include <unistd.h>
#include <string.h>
#include <minix/safecopies.h>

int
socketpath(endpoint_t endpt, const char * path, size_t size, int what,
	dev_t * dev, ino_t * ino)
{
	cp_grant_id_t grant;
	message m;
	int r;

	if ((grant = cpf_grant_direct(VFS_PROC_NR, (vir_bytes)path, size,
	    CPF_READ)) == GRANT_INVALID)
		return ENOMEM;

	memset(&m, 0, sizeof(m));
	m.m_lsys_vfs_socketpath.endpt = endpt;
	m.m_lsys_vfs_socketpath.grant = grant;
	m.m_lsys_vfs_socketpath.count = size;
	m.m_lsys_vfs_socketpath.what = what;

	r = _taskcall(VFS_PROC_NR, VFS_SOCKETPATH, &m);

	cpf_revoke(grant);

	if (r == OK) {
		*dev = m.m_vfs_lsys_socketpath.device;
		*ino = m.m_vfs_lsys_socketpath.inode;
	}

	return r;
}

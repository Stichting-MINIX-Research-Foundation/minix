#include <lib.h>
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
		return -1;	/* called function sets errno */

	memset(&m, 0, sizeof(m));
	m.VFS_CHECKPERMS_ENDPT = endpt;
	m.VFS_CHECKPERMS_GRANT = grant;
	m.VFS_CHECKPERMS_COUNT = size;

	r = _syscall(VFS_PROC_NR, VFS_CHECKPERMS, &m);

	cpf_revoke(grant);	/* does not touch errno */

	return r;
}

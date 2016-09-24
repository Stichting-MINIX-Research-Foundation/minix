#include <lib.h>
#include <string.h>
#include <minix/gcov.h>

int
gcov_flush_svr(const char * label, char * buff, size_t buff_sz)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_lc_vfs_gcov.label = (vir_bytes)label;
	m.m_lc_vfs_gcov.labellen = strlen(label) + 1;
	m.m_lc_vfs_gcov.buf = (vir_bytes)buff;
	m.m_lc_vfs_gcov.buflen = buff_sz;

	/*
	 * Make the call to VFS.  VFS will call the gcov library, buffer the
	 * stdio requests, and copy the buffer to us.
	 */
	return _syscall(VFS_PROC_NR, VFS_GCOV_FLUSH, &m);
}

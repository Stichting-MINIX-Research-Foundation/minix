#include <lib.h>
#include <string.h>
#include <minix/gcov.h>

int gcov_flush_svr(char *buff, int buff_sz, int server_nr)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_lc_vfs_gcov.buff_p = buff;
	m.m_lc_vfs_gcov.buff_sz = buff_sz;
	m.m_lc_vfs_gcov.pid = server_nr;

	/* Make the call to server. It will call the gcov library,
	 * buffer the stdio requests, and copy the buffer to this user
	 * space
	 */
	return _syscall(VFS_PROC_NR, VFS_GCOV_FLUSH, &m);
}

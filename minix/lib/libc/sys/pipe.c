#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(pipe, _pipe)
#endif

int
pipe2(int fild[2], int flags)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_lc_vfs_pipe2.flags = flags;

	if (_syscall(VFS_PROC_NR, VFS_PIPE2, &m) < 0) return(-1);
	fild[0] = m.m_lc_vfs_pipe2.fd0;
	fild[1] = m.m_lc_vfs_pipe2.fd1;
	return(0);
}

int
pipe(int fild[2])
{
	return pipe2(fild, 0);
}

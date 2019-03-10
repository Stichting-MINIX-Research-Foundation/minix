#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <unistd.h>

int
close(int fd)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_lc_vfs_close.fd = fd;

	// When closing a socket VFS would output an error message:
	// "vfs(1): panic: process has two calls (105, 131)".
	// The fix is to make close non-blocking by default.
	// There's probably a better way to do this TODO.
	m.m_lc_vfs_close.nblock = 1;

	return _syscall(VFS_PROC_NR, VFS_CLOSE, &m);
}

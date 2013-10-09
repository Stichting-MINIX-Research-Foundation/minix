#include <lib.h>
#include <string.h>

int
dupfrom(endpoint_t endpt, int fd)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.VFS_DUPFROM_ENDPT = endpt;
	m.VFS_DUPFROM_FD = fd;

	return _syscall(VFS_PROC_NR, DUPFROM, &m);
}

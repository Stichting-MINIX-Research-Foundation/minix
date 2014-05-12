#include "syslib.h"

#include <string.h>
#include <unistd.h>

int
mapdriver(char *label, devmajor_t major)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_lsys_vfs_mapdriver.label = (vir_bytes)label;
	m.m_lsys_vfs_mapdriver.labellen = strlen(label) + 1;
	m.m_lsys_vfs_mapdriver.major = major;

	return _taskcall(VFS_PROC_NR, VFS_MAPDRIVER, &m);
}

#include "syslib.h"

#include <string.h>
#include <unistd.h>

int
mapdriver(const char * label, devmajor_t major, const int * domains,
	int ndomains)
{
	message m;
	int i;

	memset(&m, 0, sizeof(m));
	m.m_lsys_vfs_mapdriver.label = (vir_bytes)label;
	m.m_lsys_vfs_mapdriver.labellen = strlen(label) + 1;
	m.m_lsys_vfs_mapdriver.major = major;
	m.m_lsys_vfs_mapdriver.ndomains = ndomains;
	if (ndomains > (int)__arraycount(m.m_lsys_vfs_mapdriver.domains))
		ndomains = (int)__arraycount(m.m_lsys_vfs_mapdriver.domains);
	for (i = 0; i < ndomains; i++)
		m.m_lsys_vfs_mapdriver.domains[i] = domains[i];

	return _taskcall(VFS_PROC_NR, VFS_MAPDRIVER, &m);
}

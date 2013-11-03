#include "syslib.h"

#include <string.h>
#include <unistd.h>

int
mapdriver(char *label, devmajor_t major)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.VFS_MAPDRIVER_LABEL = label;
	m.VFS_MAPDRIVER_LABELLEN = strlen(label) + 1;
	m.VFS_MAPDRIVER_MAJOR = major;

	return _taskcall(VFS_PROC_NR, MAPDRIVER, &m);
}

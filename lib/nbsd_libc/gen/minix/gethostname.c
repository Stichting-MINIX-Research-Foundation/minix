/* gethostname(2) system call emulation */
#include <sys/cdefs.h>
#include "namespace.h"

#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(gethostname, _gethostname)
#endif

#define HOSTNAME_FILE "/etc/hostname.file"

int gethostname(char *buf, size_t len)
{
	int fd;
	int r;
	char *nl;

	if ((fd= open(HOSTNAME_FILE, O_RDONLY)) < 0) return -1;

	r= read(fd, buf, len);
	close(fd);
	if (r == -1) return -1;

	buf[len-1]= '\0';
	if ((nl= strchr(buf, '\n')) != NULL) *nl= '\0';
	return 0;
}

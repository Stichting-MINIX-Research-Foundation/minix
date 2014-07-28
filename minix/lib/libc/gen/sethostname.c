/* gethostname(2) system call emulation */
#include <sys/cdefs.h>
#include "namespace.h"

#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <minix/paths.h>

#ifdef __weak_alias
__weak_alias(sethostname, _sethostname)
#endif

int sethostname(const char *buf, size_t len)
{
	int fd;
	int r;
	int tmperr;
	char name[20];
	strlcpy(name, "/tmp/hostname.XXXXX",sizeof(name));
	fd = mkstemp(name);

	if (fd == -1)
		return -1;

	r = fchmod(fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	if (r == -1) {
		tmperr = errno;
		close(fd);
		unlink(name);
		errno = tmperr;
		return -1;
	}

	r = write(fd, buf, len);
	tmperr = errno;
	close(fd);

	if (r == -1) {
		unlink(name);
		errno = tmperr;
		return -1;
	}

	if (r < len) {
		unlink(name);
		errno = ENOSPC;
		return -1;
	}

	r = rename(name, _PATH_HOSTNAME_FILE);

	if (r == -1) {
		tmperr = errno;
		unlink(name);
		errno = tmperr;
	}

	return 0;
}

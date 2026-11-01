/* shm.c - POSIX shared memory objects (shm_open/shm_unlink) for MINIX,
 * backed by plain files in the /dev/shm directory (mode 1777, created by
 * mtree and by etc/rc.minix as a fallback).
 *
 * Semantics follow Linux/glibc: O_NOFOLLOW|O_CLOEXEC are implied; names are
 * POSIX-style ("/name").  The usual pattern (open, unlink, ftruncate, mmap
 * MAP_SHARED) never writes data to disk: shared pages live in the VM page
 * cache.  As with a Linux tmpfs, rc.minix empties /dev/shm at boot.
 */

#include <sys/cdefs.h>
#include "namespace.h"
#include <sys/cdefs.h>
#include "namespace.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* namespace.h renames shm_open/_unlink to _shm_*; re-export public names. */
__weak_alias(shm_open, _shm_open)
__weak_alias(shm_unlink, _shm_unlink)

#define SHM_DIR	"/dev/shm"

static int
shm_path(const char *name, char *buf, size_t buflen)
{
	size_t n;

	if (name == NULL || *name == '\0') {
		errno = EINVAL;
		return -1;
	}
	if (name[0] == '/') name++;		/* POSIX requires the slash */
	if (*name == '\0' || strchr(name, '/') != NULL ||
	    strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
		errno = EINVAL;			/* no nesting per POSIX */
		return -1;
	}
	n = snprintf(buf, buflen, "%s/%s", SHM_DIR, name);
	if (n >= buflen) {
		errno = ENAMETOOLONG;
		return -1;
	}
	return 0;
}

int
shm_open(const char *name, int oflag, mode_t mode)
{
	char path[PATH_MAX];
	int fd;

	if (shm_path(name, path, sizeof(path)) < 0)
		return -1;

	fd = open(path, oflag | O_NOFOLLOW | O_CLOEXEC, mode);
	return fd;
}

int
shm_unlink(const char *name)
{
	char path[PATH_MAX];

	if (shm_path(name, path, sizeof(path)) < 0)
		return -1;
	return unlink(path);
}

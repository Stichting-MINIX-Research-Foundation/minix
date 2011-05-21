#include <sys/cdefs.h>

#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <libelf.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "_libelf.h"
#include "../libelf_compat.h"

void *
libelf_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
    void *p; /* malloc'ed pointer */
    size_t bufsize;

    if ((addr != NULL) || (flags != MAP_PRIVATE) || (prot != PROT_READ)) {
	LIBELF_SET_ERROR(ARGUMENT, 0);
	return (MAP_FAILED);
    }

    /*
     * Fall back to malloc+read.
     */
    p = NULL;
    bufsize = 1024 * 1024;
    while (/*CONSTCOND*/true) {
	void *newp = realloc(p, bufsize);
	ssize_t rsz;

	if (newp == NULL) {
	    free(p);
	    LIBELF_SET_ERROR(RESOURCE, 0);
	    return (MAP_FAILED);
	}
	p = newp;
	rsz = pread(fd, p, bufsize, 0);
	if (rsz == -1) {
	    free(p);
	    LIBELF_SET_ERROR(IO, errno);
	    return (MAP_FAILED);
	} else if ((size_t) rsz > bufsize) {
	    free(p);
	    LIBELF_SET_ERROR(IO, EIO); /* XXX */
	    return (MAP_FAILED);
	} else if ((size_t) rsz < bufsize) {
	    /*
	     * try to shrink the buffer.
	     */
	    newp = realloc(p, (size_t) rsz);
	    if (newp != NULL) {
		p = newp;
	    }
	    break;
	}
	bufsize *= 2;
    }

    return p;
}

int
libelf_munmap(void *addr, size_t len)
{
    free(addr);
    return 0;
}

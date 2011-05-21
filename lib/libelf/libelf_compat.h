#ifndef LIBELF_COMPAT_H
#define LIBELF_COMPAT_H

#if defined(NO_MMAP_FILE)

#include <stdlib.h>

#ifndef PROT_READ
#define PROT_READ       0x01    /* pages can be read */
#define	MAP_PRIVATE	0x0002	/* changes are private */

/*
 * Error indicator returned by mmap(2)
 */
#define	MAP_FAILED	((void *) -1)	/* mmap() failed */
#endif

#define mmap libelf_mmap
#define munmap libelf_munmap
void* libelf_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset);
int libelf_munmap(void *addr, size_t len);

#else /* ! NO_MMAP_FILE */

#include <sys/mman.h>

#endif /* NO_MMAP_FILE */

#endif /* LIBELF_COMPAT_H */

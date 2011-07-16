#ifndef _SYS_MMAN_H_
#define _SYS_MMAN_H_

#include <sys/featuretest.h>

#include <machine/ansi.h>

#ifdef	_BSD_SIZE_T_
typedef	_BSD_SIZE_T_	size_t;
#undef	_BSD_SIZE_T_
#endif

#include <sys/ansi.h>

#ifndef	mode_t
typedef	__mode_t	mode_t;
#define	mode_t		__mode_t
#endif

#ifndef	off_t
typedef	__off_t		off_t;		/* file offset */
#define	off_t		__off_t
#endif


/*
 * Protections are chosen from these bits, or-ed together
 */
#define PROT_NONE       0x00    /* no permissions */
#define PROT_READ       0x01    /* pages can be read */
#define PROT_WRITE      0x02    /* pages can be written */
#define PROT_EXEC       0x04    /* pages can be executed */

/*
 * Flags contain sharing type and options.
 * Sharing types; choose one.
 */
#ifndef __minix
#define	MAP_SHARED	0x0001	/* share changes */
#endif
#define	MAP_PRIVATE	0x0002	/* changes are private */

/*
 * Mapping type
 */
#define MAP_ANON	0x0004  /* anonymous memory */

/*
 * Minix specific flags.
 */
#define MAP_PREALLOC	0x0008		/* not on-demand */
#define MAP_CONTIG	0x0010		/* contiguous in physical memory */
#define MAP_LOWER16M	0x0020		/* physically below 16MB */
#define MAP_ALIGN64K	0x0040		/* physically aligned at 64kB */
#define MAP_LOWER1M	0x0080		/* physically below 16MB */
#define	MAP_ALIGNMENT_64KB	MAP_ALIGN64K
#define	MAP_IPC_SHARED	0x0100	/* share changes */

/*
 * Error indicator returned by mmap(2)
 */
#define	MAP_FAILED	((void *) -1)	/* mmap() failed */

#include <sys/cdefs.h>

__BEGIN_DECLS
#ifndef __minix
void *	mmap(void *, size_t, int, int, int, off_t);
int	munmap(void *, size_t);
#else
void *	minix_mmap(void *, size_t, int, int, int, off_t);
int	minix_munmap(void *, size_t);
int 		minix_munmap_text(void *, size_t);
void *		vm_remap(int d, int s, void *da, void *sa, size_t si);
int 		vm_unmap(int endpt, void *addr);
unsigned long 	vm_getphys(int endpt, void *addr);
u8_t 		vm_getrefcount(int endpt, void *addr);
#endif /* __minix */
__END_DECLS

#endif /* !_SYS_MMAN_H_ */

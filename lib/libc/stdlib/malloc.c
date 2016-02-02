/*	$NetBSD: malloc.c,v 1.56 2014/09/18 13:58:20 christos Exp $	*/

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * From FreeBSD: malloc.c,v 1.91 2006/01/12 07:28:20 jasone
 *
 */

#if defined(__minix)
#include "extern.h"
#ifdef _LIBSYS
#include <minix/sysutil.h>
#include <machine/param.h>
#include <machine/vmparam.h>
#define MALLOC_NO_SYSCALLS
#define wrtwarning(w) printf("libminc malloc warning: %s\n", w)
#define wrterror(w) panic("libminc malloc error: %s\n", w)
#endif
#endif /* defined(__minix) */

/*
 * Defining MALLOC_EXTRA_SANITY will enable extra checks which are related
 * to internal conditions and consistency in malloc.c. This has a
 * noticeable runtime performance hit, and generally will not do you
 * any good unless you fiddle with the internals of malloc or want
 * to catch random pointer corruption as early as possible.
 */
#ifndef MALLOC_EXTRA_SANITY
#undef MALLOC_EXTRA_SANITY
#endif

/*
 * What to use for Junk.  This is the byte value we use to fill with
 * when the 'J' option is enabled.
 */
#define SOME_JUNK	0xd0		/* as in "Duh" :-) */

/*
 * The basic parameters you can tweak.
 *
 * malloc_minsize	minimum size of an allocation in bytes.
 *			If this is too small it's too much work
 *			to manage them.  This is also the smallest
 *			unit of alignment used for the storage
 *			returned by malloc/realloc.
 *
 */

#include "namespace.h"
#if defined(__FreeBSD__)
#   if defined(__i386__)
#       define malloc_minsize		16U
#   endif
#   if defined(__ia64__)
#	define malloc_pageshift		13U
#	define malloc_minsize		16U
#   endif
#   if defined(__alpha__)
#       define malloc_pageshift		13U
#       define malloc_minsize		16U
#   endif
#   if defined(__sparc64__)
#       define malloc_pageshift		13U
#       define malloc_minsize		16U
#   endif
#   if defined(__amd64__)
#       define malloc_pageshift		12U
#       define malloc_minsize		16U
#   endif
#   if defined(__arm__)
#       define malloc_pageshift         12U
#       define malloc_minsize           16U
#   endif
#   define HAS_UTRACE
#   define UTRACE_LABEL

#include <sys/cdefs.h>
void utrace(struct ut *, int);

    /*
     * Make malloc/free/realloc thread-safe in libc for use with
     * kernel threads.
     */
#   include "libc_private.h"
#   include "spinlock.h"
    static spinlock_t thread_lock	= _SPINLOCK_INITIALIZER;
#   define _MALLOC_LOCK()		if (__isthreaded) _SPINLOCK(&thread_lock);
#   define _MALLOC_UNLOCK()		if (__isthreaded) _SPINUNLOCK(&thread_lock);
#endif /* __FreeBSD__ */

#if defined(__minix)
/* #undef these things so that malloc uses the non-internal symbols.
 * This is necessary for VM to be able to define its own versions, and
 * use this malloc.
 */
#undef mmap
#undef munmap
#endif /* defined(__minix) */

#include <sys/types.h>
#if defined(__NetBSD__)
#   define malloc_minsize               16U
#   define HAS_UTRACE
#   define UTRACE_LABEL "malloc",
#include <sys/cdefs.h>
#include "extern.h"
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: malloc.c,v 1.56 2014/09/18 13:58:20 christos Exp $");
#endif /* LIBC_SCCS and not lint */
int utrace(const char *, void *, size_t);

#include <reentrant.h>
extern int __isthreaded;
static mutex_t thread_lock = MUTEX_INITIALIZER;
#define _MALLOC_LOCK()	if (__isthreaded) mutex_lock(&thread_lock);
#define _MALLOC_UNLOCK()	if (__isthreaded) mutex_unlock(&thread_lock);
#endif /* __NetBSD__ */

#if defined(__sparc__) && defined(sun)
#   define malloc_minsize		16U
#   define MAP_ANON			(0)
    static int fdzero;
#   define MMAP_FD	fdzero
#   define INIT_MMAP() \
	{ if ((fdzero = open(_PATH_DEVZERO, O_RDWR | O_CLOEXEC, 0000)) == -1) \
	    wrterror("open of /dev/zero"); }
#endif /* __sparc__ */

/* Insert your combination here... */
#if defined(__FOOCPU__) && defined(__BAROS__)
#   define malloc_minsize		16U
#endif /* __FOOCPU__ && __BAROS__ */

#ifndef ZEROSIZEPTR
#define ZEROSIZEPTR	((void *)(uintptr_t)(1UL << (malloc_pageshift - 1)))
#endif

/*
 * No user serviceable parts behind this point.
 */
#include <sys/types.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * This structure describes a page worth of chunks.
 */

struct pginfo {
    struct pginfo	*next;	/* next on the free list */
    void		*page;	/* Pointer to the page */
    u_short		size;	/* size of this page's chunks */
    u_short		shift;	/* How far to shift for this size chunks */
    u_short		free;	/* How many free chunks */
    u_short		total;	/* How many chunk */
    u_int		bits[1]; /* Which chunks are free */
};

/*
 * This structure describes a number of free pages.
 */

struct pgfree {
    struct pgfree	*next;	/* next run of free pages */
    struct pgfree	*prev;	/* prev run of free pages */
    void		*page;	/* pointer to free pages */
    void		*end;	/* pointer to end of free pages */
    size_t		size;	/* number of bytes free */
};

/*
 * How many bits per u_int in the bitmap.
 * Change only if not 8 bits/byte
 */
#define	MALLOC_BITS	((int)(8*sizeof(u_int)))

/*
 * Magic values to put in the page_directory
 */
#define MALLOC_NOT_MINE	((struct pginfo*) 0)
#define MALLOC_FREE 	((struct pginfo*) 1)
#define MALLOC_FIRST	((struct pginfo*) 2)
#define MALLOC_FOLLOW	((struct pginfo*) 3)
#define MALLOC_MAGIC	((struct pginfo*) 4)

/*
 * Page size related parameters, computed at run-time.
 */
static size_t malloc_pagesize;
static size_t malloc_pageshift;
static size_t malloc_pagemask;

#ifndef malloc_minsize
#define malloc_minsize			16U
#endif

#ifndef malloc_maxsize
#define malloc_maxsize			((malloc_pagesize)>>1)
#endif

#define pageround(foo) (((foo) + (malloc_pagemask))&(~(malloc_pagemask)))
#define ptr2idx(foo) \
    (((size_t)(uintptr_t)(foo) >> malloc_pageshift)-malloc_origo)

#ifndef _MALLOC_LOCK
#define _MALLOC_LOCK()
#endif

#ifndef _MALLOC_UNLOCK
#define _MALLOC_UNLOCK()
#endif

#ifndef MMAP_FD
#define MMAP_FD (-1)
#endif

#ifndef INIT_MMAP
#define INIT_MMAP()
#endif

#if !defined(__minix)
#ifndef MADV_FREE
#define MADV_FREE MADV_DONTNEED
#endif
#else
#undef MADV_FREE
#endif /* !defined(__minix) */

/* Number of free pages we cache */
static size_t malloc_cache = 16;

/* The offset from pagenumber to index into the page directory */
static size_t malloc_origo;

/* The last index in the page directory we care about */
static size_t last_idx;

/* Pointer to page directory. Allocated "as if with" malloc */
static struct	pginfo **page_dir;

/* How many slots in the page directory */
static size_t	malloc_ninfo;

/* Free pages line up here */
static struct pgfree free_list;

/* Abort(), user doesn't handle problems.  */
static int malloc_abort;

/* Are we trying to die ?  */
static int suicide;

/* always realloc ?  */
static int malloc_realloc;

/* pass the kernel a hint on free pages ?  */
#if defined(MADV_FREE)
static int malloc_hint = 0;
#endif

/* xmalloc behaviour ?  */
static int malloc_xmalloc;

/* sysv behaviour for malloc(0) ?  */
static int malloc_sysv;

/* zero fill ?  */
static int malloc_zero;

/* junk fill ?  */
static int malloc_junk;

#ifdef HAS_UTRACE

/* utrace ?  */
static int malloc_utrace;

struct ut { void *p; size_t s; void *r; };

#define UTRACE(a, b, c) \
	if (malloc_utrace) {			\
		struct ut u;			\
		u.p=a; u.s = b; u.r=c;		\
		utrace(UTRACE_LABEL (void *) &u, sizeof u);	\
	}
#else /* !HAS_UTRACE */
#define UTRACE(a,b,c)
#endif /* HAS_UTRACE */

/* my last break. */
static void *malloc_brk;

/* one location cache for free-list holders */
static struct pgfree *px;

/* compile-time options */
const char *_malloc_options;

/* Name of the current public function */
static const char *malloc_func;

/* Macro for mmap */
#define MMAP(size) \
	mmap(NULL, (size), PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, \
	    MMAP_FD, (off_t)0);

/*
 * Necessary function declarations
 */
static int extend_pgdir(size_t idx);
static void *imalloc(size_t size);
static void ifree(void *ptr);
static void *irealloc(void *ptr, size_t size);

#ifndef MALLOC_NO_SYSCALLS
static void
wrtmessage(const char *p1, const char *p2, const char *p3, const char *p4)
{

    write(STDERR_FILENO, p1, strlen(p1));
    write(STDERR_FILENO, p2, strlen(p2));
    write(STDERR_FILENO, p3, strlen(p3));
    write(STDERR_FILENO, p4, strlen(p4));
}

void (*_malloc_message)(const char *p1, const char *p2, const char *p3,
	    const char *p4) = wrtmessage;
static void
wrterror(const char *p)
{

    suicide = 1;
    _malloc_message(getprogname(), malloc_func, " error: ", p);
    abort();
}

static void
wrtwarning(const char *p)
{

    /*
     * Sensitive processes, somewhat arbitrarily defined here as setuid,
     * setgid, root and wheel cannot afford to have malloc mistakes.
     */
    if (malloc_abort || issetugid() || getuid() == 0 || getgid() == 0)
	wrterror(p);
}
#endif

/*
 * Allocate a number of pages from the OS
 */
static void *
map_pages(size_t pages)
{
    caddr_t result, rresult, tail;
    intptr_t bytes = pages << malloc_pageshift;

    if (bytes < 0 || (size_t)bytes < pages) {
	errno = ENOMEM;
	return NULL;
    }

    if ((result = sbrk(bytes)) == (void *)-1)
	return NULL;

    /*
     * Round to a page, in case sbrk(2) did not do this for us
     */
    rresult = (caddr_t)pageround((size_t)(uintptr_t)result);
    if (result < rresult) {
	/* make sure we have enough space to fit bytes */
	if (sbrk((intptr_t)(rresult - result)) == (void *) -1) {
	    /* we failed, put everything back */
	    if (brk(result)) {
		wrterror("brk(2) failed [internal error]\n");
	    }
	}
    }
    tail = rresult + (size_t)bytes;

    last_idx = ptr2idx(tail) - 1;
    malloc_brk = tail;

    if ((last_idx+1) >= malloc_ninfo && !extend_pgdir(last_idx)) {
	malloc_brk = result;
	last_idx = ptr2idx(malloc_brk) - 1;
	/* Put back break point since we failed. */
	if (brk(malloc_brk))
	    wrterror("brk(2) failed [internal error]\n");
	return 0;
    }

    return rresult;
}

/*
 * Extend page directory
 */
static int
extend_pgdir(size_t idx)
{
    struct  pginfo **new, **old;
    size_t newlen, oldlen;

    /* check for overflow */
    if ((((~(1UL << ((sizeof(size_t) * NBBY) - 1)) / sizeof(*page_dir)) + 1)
	+ (malloc_pagesize / sizeof *page_dir)) < idx) {
	errno = ENOMEM;
	return 0;
    }

    /* Make it this many pages */
    newlen = pageround(idx * sizeof *page_dir) + malloc_pagesize;

    /* remember the old mapping size */
    oldlen = malloc_ninfo * sizeof *page_dir;

    /*
     * NOTE: we allocate new pages and copy the directory rather than tempt
     * fate by trying to "grow" the region.. There is nothing to prevent
     * us from accidentally re-mapping space that's been allocated by our caller
     * via dlopen() or other mmap().
     *
     * The copy problem is not too bad, as there is 4K of page index per
     * 4MB of malloc arena.
     *
     * We can totally avoid the copy if we open a file descriptor to associate
     * the anon mappings with.  Then, when we remap the pages at the new
     * address, the old pages will be "magically" remapped..  But this means
     * keeping open a "secret" file descriptor.....
     */

    /* Get new pages */
    new = MMAP(newlen);
    if (new == MAP_FAILED)
	return 0;

    /* Copy the old stuff */
    memcpy(new, page_dir, oldlen);

    /* register the new size */
    malloc_ninfo = newlen / sizeof *page_dir;

    /* swap the pointers */
    old = page_dir;
    page_dir = new;

    /* Now free the old stuff */
    munmap(old, oldlen);
    return 1;
}

/*
 * Initialize the world
 */
static void
malloc_init(void)
{
#ifndef MALLOC_NO_SYSCALLS
    const char *p;
    char b[64];
    size_t i;
    ssize_t j;
#endif
    int serrno = errno;
#ifndef MALLOC_NO_SYSCALLS

    /*
     * Compute page-size related variables.
     */
    malloc_pagesize = (size_t)sysconf(_SC_PAGESIZE);
#else
    malloc_pagesize = PAGE_SIZE;
#endif
    malloc_pagemask = malloc_pagesize - 1;
    for (malloc_pageshift = 0;
	 (1UL << malloc_pageshift) != malloc_pagesize;
	 malloc_pageshift++)
	/* nothing */ ;

    INIT_MMAP();

#ifdef MALLOC_EXTRA_SANITY
    malloc_junk = 1;
#endif /* MALLOC_EXTRA_SANITY */

#ifndef MALLOC_NO_SYSCALLS
    for (i = 0; i < 3; i++) {
	if (i == 0) {
	    j = readlink("/etc/malloc.conf", b, sizeof b - 1);
	    if (j == -1)
		continue;
	    b[j] = '\0';
	    p = b;
	} else if (i == 1 && issetugid() == 0) {
	    p = getenv("MALLOC_OPTIONS");
	} else if (i == 1) {
	    continue;
	} else {
	    p = _malloc_options;
	}
	for (; p != NULL && *p != '\0'; p++) {
	    switch (*p) {
		case '>': malloc_cache   <<= 1; break;
		case '<': malloc_cache   >>= 1; break;
		case 'a': malloc_abort   = 0; break;
		case 'A': malloc_abort   = 1; break;
#if !defined(__minix)
		case 'h': malloc_hint    = 0; break;
		case 'H': malloc_hint    = 1; break;
#endif /* !defined(__minix) */
		case 'r': malloc_realloc = 0; break;
		case 'R': malloc_realloc = 1; break;
		case 'j': malloc_junk    = 0; break;
		case 'J': malloc_junk    = 1; break;
#ifdef HAS_UTRACE
		case 'u': malloc_utrace  = 0; break;
		case 'U': malloc_utrace  = 1; break;
#endif
		case 'v': malloc_sysv    = 0; break;
		case 'V': malloc_sysv    = 1; break;
		case 'x': malloc_xmalloc = 0; break;
		case 'X': malloc_xmalloc = 1; break;
		case 'z': malloc_zero    = 0; break;
		case 'Z': malloc_zero    = 1; break;
		default:
		    _malloc_message(getprogname(), malloc_func,
			 " warning: ", "unknown char in MALLOC_OPTIONS\n");
		    break;
	    }
	}
    }
#endif

    UTRACE(0, 0, 0);

    /*
     * We want junk in the entire allocation, and zero only in the part
     * the user asked for.
     */
    if (malloc_zero)
	malloc_junk = 1;

    /* Allocate one page for the page directory */
    page_dir = MMAP(malloc_pagesize);

    if (page_dir == MAP_FAILED)
	wrterror("mmap(2) failed, check limits.\n");

    /*
     * We need a maximum of malloc_pageshift buckets, steal these from the
     * front of the page_directory;
     */
    malloc_origo = pageround((size_t)(uintptr_t)sbrk((intptr_t)0))
	>> malloc_pageshift;
    malloc_origo -= malloc_pageshift;

    malloc_ninfo = malloc_pagesize / sizeof *page_dir;

    /* Recalculate the cache size in bytes, and make sure it's nonzero */

    if (!malloc_cache)
	malloc_cache++;

    malloc_cache <<= malloc_pageshift;

    /*
     * This is a nice hack from Kaleb Keithly (kaleb@x.org).
     * We can sbrk(2) further back when we keep this on a low address.
     */
    px = imalloc(sizeof *px);

    errno = serrno;
}

/*
 * Allocate a number of complete pages
 */
static void *
malloc_pages(size_t size)
{
    void *p, *delay_free = NULL;
    size_t i;
    struct pgfree *pf;
    size_t idx;

    idx = pageround(size);
    if (idx < size) {
	errno = ENOMEM;
	return NULL;
    } else
	size = idx;

    p = NULL;

    /* Look for free pages before asking for more */
    for(pf = free_list.next; pf; pf = pf->next) {

#ifdef MALLOC_EXTRA_SANITY
	if (pf->size & malloc_pagemask)
	    wrterror("(ES): junk length entry on free_list.\n");
	if (!pf->size)
	    wrterror("(ES): zero length entry on free_list.\n");
	if (pf->page == pf->end)
	    wrterror("(ES): zero entry on free_list.\n");
	if (pf->page > pf->end)
	    wrterror("(ES): sick entry on free_list.\n");
	if ((void*)pf->page >= (void*)sbrk(0))
	    wrterror("(ES): entry on free_list past brk.\n");
	if (page_dir[ptr2idx(pf->page)] != MALLOC_FREE)
	    wrterror("(ES): non-free first page on free-list.\n");
	if (page_dir[ptr2idx(pf->end)-1] != MALLOC_FREE)
	    wrterror("(ES): non-free last page on free-list.\n");
#endif /* MALLOC_EXTRA_SANITY */

	if (pf->size < size)
	    continue;

	if (pf->size == size) {
	    p = pf->page;
	    if (pf->next != NULL)
		    pf->next->prev = pf->prev;
	    pf->prev->next = pf->next;
	    delay_free = pf;
	    break;
	} 

	p = pf->page;
	pf->page = (char *)pf->page + size;
	pf->size -= size;
	break;
    }

#ifdef MALLOC_EXTRA_SANITY
    if (p != NULL && page_dir[ptr2idx(p)] != MALLOC_FREE)
	wrterror("(ES): allocated non-free page on free-list.\n");
#endif /* MALLOC_EXTRA_SANITY */

    size >>= malloc_pageshift;

    /* Map new pages */
    if (p == NULL)
	p = map_pages(size);

    if (p != NULL) {

	idx = ptr2idx(p);
	page_dir[idx] = MALLOC_FIRST;
	for (i=1;i<size;i++)
	    page_dir[idx+i] = MALLOC_FOLLOW;

	if (malloc_junk)
	    memset(p, SOME_JUNK, size << malloc_pageshift);
    }

    if (delay_free) {
	if (px == NULL)
	    px = delay_free;
	else
	    ifree(delay_free);
    }

    return p;
}

/*
 * Allocate a page of fragments
 */

static inline int
malloc_make_chunks(int bits)
{
    struct  pginfo *bp;
    void *pp;
    int i, k;
    long l;

    /* Allocate a new bucket */
    pp = malloc_pages(malloc_pagesize);
    if (pp == NULL)
	return 0;

    /* Find length of admin structure */
    l = (long)offsetof(struct pginfo, bits[0]);
    l += (long)sizeof bp->bits[0] *
	(((malloc_pagesize >> bits)+MALLOC_BITS-1) / MALLOC_BITS);

    /* Don't waste more than two chunks on this */
    if ((1<<(bits)) <= l+l) {
	bp = (struct  pginfo *)pp;
    } else {
	bp = imalloc((size_t)l);
	if (bp == NULL) {
	    ifree(pp);
	    return 0;
	}
    }

    bp->size = (1<<bits);
    bp->shift = bits;
    bp->total = bp->free = (u_short)(malloc_pagesize >> bits);
    bp->page = pp;

    /* set all valid bits in the bitmap */
    k = bp->total;
    i = 0;

    /* Do a bunch at a time */
    for(;k-i >= MALLOC_BITS; i += MALLOC_BITS)
	bp->bits[i / MALLOC_BITS] = ~0U;

    for(; i < k; i++)
        bp->bits[i/MALLOC_BITS] |= 1<<(i%MALLOC_BITS);

    if (bp == bp->page) {
	/* Mark the ones we stole for ourselves */
	for(i = 0; l > 0; i++) {
	    bp->bits[i / MALLOC_BITS] &= ~(1 << (i % MALLOC_BITS));
	    bp->free--;
	    bp->total--;
	    l -= (long)(1 << bits);
	}
    }

    /* MALLOC_LOCK */

    page_dir[ptr2idx(pp)] = bp;

    bp->next = page_dir[bits];
    page_dir[bits] = bp;

    /* MALLOC_UNLOCK */

    return 1;
}

/*
 * Allocate a fragment
 */
static void *
malloc_bytes(size_t size)
{
    size_t i;
    int j;
    u_int u;
    struct  pginfo *bp;
    size_t k;
    u_int *lp;

    /* Don't bother with anything less than this */
    if (size < malloc_minsize)
	size = malloc_minsize;


    /* Find the right bucket */
    j = 1;
    i = size-1;
    while (i >>= 1)
	j++;

    /* If it's empty, make a page more of that size chunks */
    if (page_dir[j] == NULL && !malloc_make_chunks(j))
	return NULL;

    bp = page_dir[j];

    /* Find first word of bitmap which isn't empty */
    for (lp = bp->bits; !*lp; lp++)
	;

    /* Find that bit, and tweak it */
    u = 1;
    k = 0;
    while (!(*lp & u)) {
	u += u;
	k++;
    }
    *lp ^= u;

    /* If there are no more free, remove from free-list */
    if (!--bp->free) {
	page_dir[j] = bp->next;
	bp->next = NULL;
    }

    /* Adjust to the real offset of that chunk */
    k += (lp-bp->bits)*MALLOC_BITS;
    k <<= bp->shift;

    if (malloc_junk)
	memset((u_char*)bp->page + k, SOME_JUNK, (size_t)bp->size);

    return (u_char *)bp->page + k;
}

/*
 * Allocate a piece of memory
 */
static void *
imalloc(size_t size)
{
    void *result;

    if (suicide)
	abort();

    if ((size + malloc_pagesize) < size)	/* Check for overflow */
	result = NULL;
#ifndef __minix
    else if ((size + malloc_pagesize) >= (uintptr_t)page_dir)
	result = NULL;
#endif /* !__minix */
    else if (size <= malloc_maxsize)
	result = malloc_bytes(size);
    else
	result = malloc_pages(size);

    if (malloc_abort && result == NULL)
	wrterror("allocation failed.\n");

    if (malloc_zero && result != NULL)
	memset(result, 0, size);

    return result;
}

/*
 * Change the size of an allocation.
 */
static void *
irealloc(void *ptr, size_t size)
{
    void *p;
    size_t osize, idx;
    struct pginfo **mp;
    size_t i;

    if (suicide)
	abort();

    idx = ptr2idx(ptr);

    if (idx < malloc_pageshift) {
	wrtwarning("junk pointer, too low to make sense.\n");
	return 0;
    }

    if (idx > last_idx) {
	wrtwarning("junk pointer, too high to make sense.\n");
	return 0;
    }

    mp = &page_dir[idx];

    if (*mp == MALLOC_FIRST) {			/* Page allocation */

	/* Check the pointer */
	if ((size_t)(uintptr_t)ptr & malloc_pagemask) {
	    wrtwarning("modified (page-) pointer.\n");
	    return NULL;
	}

	/* Find the size in bytes */
	for (osize = malloc_pagesize; *++mp == MALLOC_FOLLOW;)
	    osize += malloc_pagesize;

        if (!malloc_realloc && 			/* unless we have to, */
	  size <= osize && 			/* .. or are too small, */
	  size > (osize - malloc_pagesize)) {	/* .. or can free a page, */
	    if (malloc_junk)
		memset((u_char *)ptr + size, SOME_JUNK, osize-size);
	    return ptr;				/* don't do anything. */
	}

    } else if (*mp >= MALLOC_MAGIC) {		/* Chunk allocation */

	/* Check the pointer for sane values */
	if (((size_t)(uintptr_t)ptr & ((*mp)->size-1))) {
	    wrtwarning("modified (chunk-) pointer.\n");
	    return NULL;
	}

	/* Find the chunk index in the page */
	i = ((size_t)(uintptr_t)ptr & malloc_pagemask) >> (*mp)->shift;

	/* Verify that it isn't a free chunk already */
        if ((*mp)->bits[i/MALLOC_BITS] & (1UL << (i % MALLOC_BITS))) {
	    wrtwarning("chunk is already free.\n");
	    return NULL;
	}

	osize = (*mp)->size;

	if (!malloc_realloc &&		/* Unless we have to, */
	  size <= osize && 		/* ..or are too small, */
	  (size > osize / 2 ||	 	/* ..or could use a smaller size, */
	  osize == malloc_minsize)) {	/* ..(if there is one) */
	    if (malloc_junk)
		memset((u_char *)ptr + size, SOME_JUNK, osize-size);
	    return ptr;			/* ..Don't do anything */
	}

    } else {
	wrtwarning("pointer to wrong page.\n");
	return NULL;
    }

    p = imalloc(size);

    if (p != NULL) {
	/* copy the lesser of the two sizes, and free the old one */
	if (!size || !osize)
	    ;
	else if (osize < size)
	    memcpy(p, ptr, osize);
	else
	    memcpy(p, ptr, size);
	ifree(ptr);
    } 
    return p;
}

/*
 * Free a sequence of pages
 */

static inline void
free_pages(void *ptr, size_t idx, struct pginfo *info)
{
    size_t i;
    struct pgfree *pf, *pt=NULL;
    size_t l;
    void *tail;

    if (info == MALLOC_FREE) {
	wrtwarning("page is already free.\n");
	return;
    }

    if (info != MALLOC_FIRST) {
	wrtwarning("pointer to wrong page.\n");
	return;
    }

    if ((size_t)(uintptr_t)ptr & malloc_pagemask) {
	wrtwarning("modified (page-) pointer.\n");
	return;
    }

    /* Count how many pages and mark them free at the same time */
    page_dir[idx] = MALLOC_FREE;
    for (i = 1; page_dir[idx+i] == MALLOC_FOLLOW; i++)
	page_dir[idx + i] = MALLOC_FREE;

    l = i << malloc_pageshift;

    if (malloc_junk)
	memset(ptr, SOME_JUNK, l);

#if !defined(__minix)
    if (malloc_hint)
	madvise(ptr, l, MADV_FREE);
#endif /* !defined(__minix) */

    tail = (char *)ptr+l;

    /* add to free-list */
    if (px == NULL)
	px = imalloc(sizeof *px);	/* This cannot fail... */
    px->page = ptr;
    px->end =  tail;
    px->size = l;
    if (free_list.next == NULL) {

	/* Nothing on free list, put this at head */
	px->next = free_list.next;
	px->prev = &free_list;
	free_list.next = px;
	pf = px;
	px = NULL;

    } else {

	/* Find the right spot, leave pf pointing to the modified entry. */
	tail = (char *)ptr+l;

	for(pf = free_list.next; pf->end < ptr && pf->next != NULL;
	    pf = pf->next)
	    ; /* Race ahead here */

	if (pf->page > tail) {
	    /* Insert before entry */
	    px->next = pf;
	    px->prev = pf->prev;
	    pf->prev = px;
	    px->prev->next = px;
	    pf = px;
	    px = NULL;
	} else if (pf->end == ptr ) {
	    /* Append to the previous entry */
	    pf->end = (char *)pf->end + l;
	    pf->size += l;
	    if (pf->next != NULL && pf->end == pf->next->page ) {
		/* And collapse the next too. */
		pt = pf->next;
		pf->end = pt->end;
		pf->size += pt->size;
		pf->next = pt->next;
		if (pf->next != NULL)
		    pf->next->prev = pf;
	    }
	} else if (pf->page == tail) {
	    /* Prepend to entry */
	    pf->size += l;
	    pf->page = ptr;
	} else if (pf->next == NULL) {
	    /* Append at tail of chain */
	    px->next = NULL;
	    px->prev = pf;
	    pf->next = px;
	    pf = px;
	    px = NULL;
	} else {
	    wrterror("freelist is destroyed.\n");
	}
    }
    
    /* Return something to OS ? */
    if (pf->next == NULL &&			/* If we're the last one, */
      pf->size > malloc_cache &&		/* ..and the cache is full, */
      pf->end == malloc_brk &&			/* ..and none behind us, */
      malloc_brk == sbrk((intptr_t)0)) {	/* ..and it's OK to do... */

	/*
	 * Keep the cache intact.  Notice that the '>' above guarantees that
	 * the pf will always have at least one page afterwards.
	 */
	pf->end = (char *)pf->page + malloc_cache;
	pf->size = malloc_cache;

	brk(pf->end);
	malloc_brk = pf->end;

	idx = ptr2idx(pf->end);

	for(i=idx;i <= last_idx;)
	    page_dir[i++] = MALLOC_NOT_MINE;

	last_idx = idx - 1;

	/* XXX: We could realloc/shrink the pagedir here I guess. */
    }
    if (pt != NULL)
	ifree(pt);
}

/*
 * Free a chunk, and possibly the page it's on, if the page becomes empty.
 */

static inline void
free_bytes(void *ptr, size_t idx, struct pginfo *info)
{
    size_t i;
    struct pginfo **mp;
    void *vp;

    /* Find the chunk number on the page */
    i = ((size_t)(uintptr_t)ptr & malloc_pagemask) >> info->shift;

    if (((size_t)(uintptr_t)ptr & (info->size-1))) {
	wrtwarning("modified (chunk-) pointer.\n");
	return;
    }

    if (info->bits[i/MALLOC_BITS] & (1UL << (i % MALLOC_BITS))) {
	wrtwarning("chunk is already free.\n");
	return;
    }

    if (malloc_junk)
	memset(ptr, SOME_JUNK, (size_t)info->size);

    info->bits[i/MALLOC_BITS] |= (u_int)(1UL << (i % MALLOC_BITS));
    info->free++;

    mp = page_dir + info->shift;

    if (info->free == 1) {

	/* Page became non-full */

	mp = page_dir + info->shift;
	/* Insert in address order */
	while (*mp && (*mp)->next && (*mp)->next->page < info->page)
	    mp = &(*mp)->next;
	info->next = *mp;
	*mp = info;
	return;
    }

    if (info->free != info->total)
	return;

    /* Find & remove this page in the queue */
    while (*mp != info) {
	mp = &((*mp)->next);
#ifdef MALLOC_EXTRA_SANITY
	if (!*mp)
		wrterror("(ES): Not on queue.\n");
#endif /* MALLOC_EXTRA_SANITY */
    }
    *mp = info->next;

    /* Free the page & the info structure if need be */
    page_dir[idx] = MALLOC_FIRST;
    vp = info->page;		/* Order is important ! */
    if(vp != (void*)info) 
	ifree(info);
    ifree(vp);
}

static void
ifree(void *ptr)
{
    struct pginfo *info;
    size_t idx;

    /* This is legal */
    if (ptr == NULL)
	return;

    /* If we're already sinking, don't make matters any worse. */
    if (suicide)
	return;

    idx = ptr2idx(ptr);

    if (idx < malloc_pageshift) {
	wrtwarning("junk pointer, too low to make sense.\n");
	return;
    }

    if (idx > last_idx) {
	wrtwarning("junk pointer, too high to make sense.\n");
	return;
    }

    info = page_dir[idx];

    if (info < MALLOC_MAGIC)
        free_pages(ptr, idx, info);
    else
	free_bytes(ptr, idx, info);
    return;
}

static int malloc_active; /* Recursion flag for public interface. */
static unsigned malloc_started; /* Set when initialization has been done */

static void *
pubrealloc(void *ptr, size_t size, const char *func)
{
    void *r;
    int err = 0;

    /*
     * If a thread is inside our code with a functional lock held, and then
     * catches a signal which calls us again, we would get a deadlock if the
     * lock is not of a recursive type.
     */
    _MALLOC_LOCK();
    malloc_func = func;
    if (malloc_active > 0) {
	if (malloc_active == 1) {
	    wrtwarning("recursive call\n");
	    malloc_active = 2;
	}
        _MALLOC_UNLOCK();
	errno = EINVAL;
	return (NULL);
    } 
    malloc_active = 1;

    if (!malloc_started) {
        if (ptr != NULL) {
	    wrtwarning("malloc() has never been called\n");
	    malloc_active = 0;
            _MALLOC_UNLOCK();
	    errno = EINVAL;
	    return (NULL);
	}
	malloc_init();
	malloc_started = 1;
    }
   
    if (ptr == ZEROSIZEPTR)
	ptr = NULL;
    if (malloc_sysv && !size) {
	if (ptr != NULL)
	    ifree(ptr);
	r = NULL;
    } else if (!size) {
	if (ptr != NULL)
	    ifree(ptr);
	r = ZEROSIZEPTR;
    } else if (ptr == NULL) {
	r = imalloc(size);
	err = (r == NULL);
    } else {
        r = irealloc(ptr, size);
	err = (r == NULL);
    }
    UTRACE(ptr, size, r);
    malloc_active = 0;
    _MALLOC_UNLOCK();
    if (malloc_xmalloc && err)
	wrterror("out of memory\n");
    if (err)
	errno = ENOMEM;
    return (r);
}

/*
 * These are the public exported interface routines.
 */

void *
malloc(size_t size)
{

    return pubrealloc(NULL, size, " in malloc():");
}

int
posix_memalign(void **memptr, size_t alignment, size_t size)
{
    int err;
    void *result;

    if (!malloc_started) {
	    malloc_init();
	    malloc_started = 1;
    }
    /* Make sure that alignment is a large enough power of 2. */
    if (((alignment - 1) & alignment) != 0 || alignment < sizeof(void *) ||
	alignment > malloc_pagesize)
	    return EINVAL;

    /* 
     * (size | alignment) is enough to assure the requested alignment, since
     * the allocator always allocates power-of-two blocks.
     */
    err = errno; /* Protect errno against changes in pubrealloc(). */
    result = pubrealloc(NULL, (size | alignment), " in posix_memalign()");
    errno = err;

    if (result == NULL)
	return ENOMEM;

    *memptr = result;
    return 0;
}

void *
calloc(size_t num, size_t size)
{
    void *ret;

    if (size != 0 && (num * size) / size != num) {
	/* size_t overflow. */
	errno = ENOMEM;
	return (NULL);
    }

    ret = pubrealloc(NULL, num * size, " in calloc():");

    if (ret != NULL)
	memset(ret, 0, num * size);

    return ret;
}

void
free(void *ptr)
{

    pubrealloc(ptr, 0, " in free():");
}

void *
realloc(void *ptr, size_t size)
{

    return pubrealloc(ptr, size, " in realloc():");
}

/*
 * Begin library-private functions, used by threading libraries for protection
 * of malloc during fork().  These functions are only called if the program is
 * running in threaded mode, so there is no need to check whether the program
 * is threaded here.
 */

void
_malloc_prefork(void)
{

	_MALLOC_LOCK();
}

void
_malloc_postfork(void)
{

	_MALLOC_UNLOCK();
}

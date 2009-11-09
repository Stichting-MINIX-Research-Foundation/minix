/* Copyright (c) 1985 Ceriel J.H. Jacobs */

# ifndef lint
static char rcsid[] = "$Header$";
# endif

# define _GETLINE_

# include <errno.h>
# include "in_all.h"
# include "getline.h"
# include "options.h"
# include "process.h"
# include "term.h"
# include "main.h"
# include "display.h"
# include "output.h"
# include "assert.h"

extern int errno;

# define BLOCKSIZE 2048		/* size of blocks */
# define CHUNK 50		/* # of blockheaders allocated at a time */

/*
 * The blockheaders of the blocks that are in core are kept in a linked list.
 * The last added block is indicated by b_head,
 * the tail of the list is indicated by b_tail.
 * The links go from b_tail to b_head.
 * The blockheaders are all in an array, in the order of the line numbers.
 * Also, the blockheaders must always be in core, so they have to be rather
 * small. On systems with a small address space, yap can run out of core,
 * and panic. However, this should only happen with very large files (>> 1M).
 */

struct block {
	int		b_flags;	/* Contains the following flags: */
# define DUMPED		01		/* block dumped on temporary file */
# define PARTLY		02		/* block not filled completely (eof) */
	int		b_next;		/* ptr in linked list */
	long		b_end;		/* line number of last line in block */
	char	 *	b_info;		/* the block */
	int      *	b_offs;		/* line offsets within the block */
	long		b_foff;		/* offset of block in file */
};

static struct block *	blocklist,	/* beginning of the list of blocks */
		    *	maxblocklist,	/* first free entry in the list */
		    *	topblocklist;	/* end of allocated core for the list */
static int	b_head,
		b_tail;
static int	tfdes, ifdes;		/* File descriptors for temporary's */
static long	lastreadline;		/* lineno of last line read */
static int	ENDseen;

STATIC VOID readblock();
STATIC VOID nextblock();
STATIC char *re_alloc();

STATIC struct block *
new_block()
{
	register struct block *pblock = maxblocklist - 1;

	if (!maxblocklist || !(pblock->b_flags & PARTLY)) {
		/*
		 * There is no last block, or it was filled completely,
		 * so allocate a new blockheader.
		 */
		register int siz;

		pblock = blocklist;
		if (maxblocklist == topblocklist) {
			/*
			 * No blockheaders left. Allocate new ones
			 */
			siz = topblocklist - pblock;
			blocklist = pblock = (struct block *)
			re_alloc((char *) pblock,
			(unsigned) (siz * sizeof(*pblock)),
			(unsigned) ((siz + CHUNK) * sizeof(*pblock)));
			pblock += siz;
			topblocklist = pblock + CHUNK;
			maxblocklist = pblock;
			for (; pblock < topblocklist; pblock++) {
				pblock->b_end = 0;
				pblock->b_info = 0;
				pblock->b_flags = 0;
			}
			if (!siz) {
				/*
				 * Create dummy header cell.
				 */
				maxblocklist++;
			}
		}
		pblock = maxblocklist++;
	}
	nextblock(pblock);
	return pblock;
}

/*
 * Return the block in which line 'n' of the current file can be found.
 * If "disable_interrupt" = 0, the call may be interrupted, in which
 * case it returns 0.
 */

STATIC struct block *
getblock(n, disable_interrupt) register long n; {
	register struct block * pblock;

	if (stdf < 0) {
		/*
		 * Not file descriptor, so return end of file
		 */
		return 0;
	}
	pblock = maxblocklist - 1;
	if (n < lastreadline ||
	    (n == lastreadline && !(pblock->b_flags & PARTLY))) {
		/*
		 * The line asked for has been read already.
		 * Perform binary search in the blocklist to find the block
		 * where it's in.
		 */
		register struct block *min, *mid;

		min = blocklist + 1;
		do {
			mid = min + (pblock - min) / 2;
			if (n > mid->b_end) {
				min = mid + 1;
			}
			else pblock = mid;
		} while (min < pblock);
		/* Found, pblock is now a reference to the block wanted */
		if (!pblock->b_info) readblock(pblock);
		return pblock;
	}

	/*
	 * The line was'nt read yet, so read blocks until found
	 */
	for (;;) {
		if (interrupt && !disable_interrupt) return 0;
		pblock = new_block();
		if (pblock->b_end >= n) {
			return pblock;
		}
		if (pblock->b_flags & PARTLY) {
			/*
			 * We did not find it, and the last block could not be
			 * read completely, so return 0;
			 */
			return	0;
		}
	}
	/* NOTREACHED */
}

char *
getline(n, disable_interrupt) long n; {
	register struct block *pblock;

	if (!(pblock = getblock(n, disable_interrupt))) {
		return (char *) 0;
	}
	return pblock->b_info + pblock->b_offs[n - ((pblock-1)->b_end + 1)];
}

/*
 * Find the last line of the input, and return its number
 */

long
to_lastline() {

	for (;;) {
		if (!getline(lastreadline + 1, 0)) {
			/*
			 * "lastreadline" always contains the linenumber of
			 * the last line read. So, if the call to getline
			 * succeeds, "lastreadline" is affected
			 */
			if (interrupt) return -1L;
			return lastreadline;
		}
	}
	/* NOTREACHED */
}

#if MAXNBLOCKS
int nblocks;		/* Count number of large blocks */
#endif

/*
 * Allocate some memory. If unavailable, free some and try again.
 * If all fails, panic.
 */

char *
alloc(size, isblock) unsigned size; {

	register char *pmem;
	register struct block *pblock, *bllist;
	char *malloc();
	long lseek();
	register long i;

	bllist = blocklist;
	while (
#if MAXNBLOCKS
	   (isblock && nblocks >= MAXNBLOCKS) ||
#endif
	   !(pmem = malloc(size))   /* No space */
	) {
		if (b_tail == 0) {
			/*
			 * Also, no blocks in core. Pity
			 */
			panic("No core");
		}
#if MAXNBLOCKS
		nblocks--;
#endif
		pblock = bllist + b_tail;
		b_tail = pblock->b_next;
		if (!nopipe && !(pblock->b_flags & DUMPED)) {
			/*
			 * Dump the block on a temporary file
			 */
			if (!tfdes) {
				/*
				 * create and open temporary files
				 */
				tfdes = opentemp(0);
				ifdes = opentemp(1);
			}
			pblock->b_flags |= DUMPED;
			/*
			 * Find out where to dump the block, and dump it
			 */
			i = (pblock-1)->b_end * sizeof(int);
			(VOID) lseek(tfdes,
				((long) BLOCKSIZE * (pblock - bllist)), 0);
			if (write(tfdes, pblock->b_info, BLOCKSIZE)
			    != BLOCKSIZE) {
				panic("write failed");
			}
			/*
			 * Also dump the offsets of the lines in the block
			 */
			(VOID) lseek(ifdes, i, 0);
			i = pblock->b_end * sizeof(int) - i;
			if (write(ifdes, (char *) pblock->b_offs, (int) i)
			    != (int) i) {
				panic("Write failed");
			}
		}
		/*
		 * Now that the block is dumped, the space taken by it can
		 * be freed
		 */
		free((char *) pblock->b_offs);
		free(pblock->b_info);
		pblock->b_info = (char *) 0;
	}
#if MAXNBLOCKS
	if (isblock) nblocks++;
#endif
	return pmem;
}

/*
 * Re-allocate the memorychunk pointed to by ptr, to let it
 * grow or shrink.
 * realloc of the standard C library is useless, as it is destructive
 * if the malloc fails.
 */

STATIC char *
re_alloc(ptr,oldsize, newsize)
char *ptr; unsigned oldsize; unsigned newsize; {
	register char *pmem;
	register char *c1, *c2;

	/*
	 * We could be smarter here, by checking if newsize < oldsize, and in
	 * that case using realloc, but this depends on realloc using the
	 * same block if the block shrinks. The question is, wether all
	 * reallocs in the world do this.
	 */
	pmem = alloc(newsize, 0);
	if (oldsize) {
		/*
		 * This test makes re_alloc also work if there was no old block
		 */
		c1 = pmem;
		c2 = ptr;
		if (newsize > oldsize) {
			newsize = oldsize;
		}
		while (newsize--) {
			*c1++ = *c2++;
		}
		free(ptr);
	}
	return pmem;
}

/*
 * Append a block to the linked list of blockheaders of blocks that are
 * in core.
 */

STATIC VOID
addtolist(pblock) register struct block *pblock; {
	register struct block *bllist = blocklist;

	pblock->b_next = 0;
	(bllist + b_head)->b_next = pblock - bllist;
	b_head = pblock - bllist;
	if (!b_tail) {
		/*
		 * The list was empty, initialize
		 */
		b_tail = b_head;
	}
}

static char *saved;
static long filldegree;

/*
 * Try to read the block indicated by pblock
 */

STATIC VOID
nextblock(pblock) register struct block *pblock; {
	register char *c,	/* Run through pblock->b_info */
		      *c1;	/* indicate end of pblock->b_info */
	register int *poff;	/* pointer in line-offset list */
	register int cnt;	/* # of characters read */
	register unsigned siz;	/* Size of allocated line-offset list */
	static unsigned savedsiz;	/* saved "siz" */
	static int *savedpoff;		/* saved "poff" */
	static char *savedc1;		/* saved "c1" */

	if (pblock->b_flags & PARTLY) {
		/*
		 * The block was already partly filled. Initialize locals
		 * accordingly
		 */
		poff = savedpoff;
		siz = savedsiz;
		pblock->b_flags = 0;
		c1 = savedc1;
		if (c1 == pblock->b_info || *(c1 - 1)) {
			/*
			 * We had incremented "lastreadline" temporarily,
			 * because the last line could not be completely read
			 * last time we tried. Undo this increment
			 */
			poff--;
			--lastreadline;
		}
	}
	else {
		if (nopipe) pblock->b_foff = lseek(stdf, 0L, 1);
		if (saved) {
			/*
			 * There were leftovers from the previous block
			 */
			pblock->b_info = saved;
			if (nopipe) pblock->b_foff -= savedc1 - saved;
			c1 = savedc1;
			saved = 0;
		}
		else {	/* Allocate new block */
			pblock->b_info = c1 = alloc(BLOCKSIZE + 1, 1);
		}
		/*
		 * Allocate some space for line-offsets
		 */
		pblock->b_offs = poff = (int *)
			alloc((unsigned) (100 * sizeof(int)), 0);
		siz = 99;
		*poff++ = 0;
	}
	c = c1;
	for (;;) {
		/*
		 * Read loop
		 */
		cnt = read(stdf, c1, BLOCKSIZE - (c1 - pblock->b_info));
		if (cnt < 0) {
			/*
			 * Interrupted read
			 */
			if (errno == EINTR) continue;
			error("Could not read input file");
			cnt = 0;
		}
		c1 += cnt;
		if (c1 != pblock->b_info + BLOCKSIZE) {
			ENDseen = 1;
			pblock->b_flags |= PARTLY;
		}
		break;
	}
	assert(c <= c1);
	while (c < c1) {
		/*
		 * Now process the block
		 */
		*c &= 0177;	/* Most significant bit ignored */
		if (*c == '\n') {
			/*
			 * Newlines are replaced by '\0', so that "getline"
			 * can deliver one line at a time
			 */
			*c = 0;
			lastreadline++;
			/*
			 * Remember the line-offset
			 */
			if (poff == pblock->b_offs + siz) {
				/*
				 * No space for it, allocate some more
				 */
				pblock->b_offs = (int *)
					re_alloc((char *) pblock->b_offs,
						 (siz+1) * sizeof(int),
						 (siz + 51) * sizeof(int));
				poff = pblock->b_offs + siz;
				siz += 50;
			}
			*poff++ = c - pblock->b_info + 1;
		}
		else if (*c == '\0') {
			/*
			 * 0-bytes are replaced by 0200, because newlines are
			 * replaced by 0, and 0200 & 0177 gives again 0 ...
			 */
			*c = 0200;
		}
		c++;
	}
	assert(c==c1);
	*c = 0;
	if (c != pblock->b_info && *(c-1) != 0) {
		/*
		 * The last line read does not end with a newline, so add one
		 */
		lastreadline++;
		*poff++ = c - pblock->b_info + 1;
		if (!(pblock->b_flags & PARTLY) && *(poff - 2) != 0) {
			/*
			 * Save the started line; it will be in the next block.
			 * Remove the newline we added just now.
			 */
			saved = c1 = alloc(BLOCKSIZE + 1, 1);
			c = pblock->b_info + *(--poff - 1);
			while (*c) *c1++ = *c++;
			c = pblock->b_info + *(poff - 1);
			savedc1 = c1;
			--lastreadline;
		}
	}
	pblock->b_end = lastreadline;
	if (pblock->b_flags & PARTLY) {
		/*
		 * Take care, that we can call "nextblock" again, to fill in
		 * the rest of this block
		 */
		savedsiz = siz;
		savedpoff = poff;
		savedc1 = c;
		if (c == pblock->b_info) {
			lastreadline++;
			pblock->b_end = 0;
		}
	}
	else {
		/*
		 * Not completely read blocks are not in the linked list,
		 * so can never be "swapped out".
		 */
		addtolist(pblock);
		cnt = pblock - blocklist;
		filldegree = ((c-pblock->b_info) + (cnt-1) * filldegree) / cnt;
	}
	assert(pblock->b_end - (pblock-1)->b_end <= poff - pblock->b_offs);
}

/*
 * Allocate core for the block, and read it back from
 * the temporary file.
 */

STATIC VOID
readblock(pblock) register struct block *pblock; {

	register int size;
	register long i;

	/*
	 * Find out where the block is, and read it
	 */
	pblock->b_info = alloc(BLOCKSIZE + 1, 1);
	i = (pblock - 1)->b_end * sizeof(int);
	size = (int) (pblock->b_end * sizeof(int) - i);
	pblock->b_offs	= (int *) alloc((unsigned) size, 0);
	if (nopipe) {
		register char *c;
		register int line_index;
		int cnt;
		long l = lseek(stdf, 0L, 1);

		(VOID) lseek(stdf, pblock->b_foff, 0);
		cnt = read(stdf, pblock->b_info, BLOCKSIZE);
		(VOID) lseek(stdf, l, 0);
		c = pblock->b_info;
		pblock->b_offs[0] = 0;
		line_index = 1;
		size /= sizeof(int);
		while (c < pblock->b_info + cnt) {
			*c &= 0177;
			if (*c == '\n') {
				*c = '\0';
				if (line_index < size)
					pblock->b_offs[line_index++] =
						(c - pblock->b_info) + 1;
			}
			else if (*c == '\0') *c = 0200;
			c++;
		}
		*c = '\0';
	}
	else {
		(VOID) lseek(tfdes, (long) ((long) BLOCKSIZE * (pblock - blocklist)),0);
		if (read(tfdes, pblock->b_info,BLOCKSIZE) != BLOCKSIZE) {
			panic("read error");
		}
		/*
		 * Find out where the line-offset list is, and read it
		 */
		(VOID) lseek(ifdes, i, 0);
		if (read(ifdes, (char *) pblock->b_offs, size) != size) {
			panic("read error");
		}
		pblock->b_info[BLOCKSIZE] = '\0';
	}
	/*
	 * Add this block to the list of incore blocks
	 */
	addtolist(pblock);
}

/*
 * Called after processing a file.
 * Free all core.
 */

VOID
do_clean() {

	register struct block *pblock;
	register char *p;

	for (pblock = blocklist; pblock < maxblocklist; pblock++) {
		if (p = pblock->b_info) {
			free(p);
			free((char *) pblock->b_offs);
		}
	}
	if (p = (char *) blocklist) {
		free(p);
	}
	blocklist = 0;
	maxblocklist = 0;
	topblocklist = 0;
	lastreadline = 0;
	filldegree = 0;
	ENDseen = 0;
	if (p = saved) free(p);
	saved = 0;
	b_head = 0;
	b_tail = 0;
# if MAXNBLOCKS
	nblocks = 0;
# endif
}

/*
 * Close a file with file-descriptor "file", if it indeed is one
 */

STATIC VOID
cls(file) {
	if (file) (VOID) close(file);
}

/*
 * Close all files
 */

VOID
cls_files() {

	cls(tfdes);
	cls(ifdes);
	cls(stdf);
}

/*
 * Get a character. If possible, do some workahead.
 */

int
getch() {
# if USG_OPEN
# include <fcntl.h>
# include <sys/stat.h>

	register int i,j;
	struct stat buf;
# else
# ifdef FIONREAD
# include <sys/stat.h>

	struct stat buf;
	long i;
# endif
# endif

	char c;
	int retval;

	flush();
	if (startcomm) {
		/*
		 * Command line option command
		 */
		if (*startcomm) return *startcomm++;
		return '\n';
	}
# if USG_OPEN
	if (stdf >= 0) {
		/*
		 * Make reads from the terminal non-blocking, so that
		 * we can see if the user typed something
		 */
		i = fcntl(0,F_GETFL,0);
		if (i != -1 && fcntl(0, F_SETFL, i|O_NDELAY) != -1) {
			j = 0;
			while (! ENDseen && 
			       ((j = read(0,&c,1)) == 0
#ifdef EWOULDBLOCK
			        || (j < 0 && errno == EWOULDBLOCK)
#endif
			       )
			       &&
			       (nopipe || 
				(fstat(stdf,&buf) >= 0 && buf.st_size > 0))) {
				/*
				 * Do some read ahead, after making sure there
				 * is input and the user did not type a command
				 */
				new_block();
			}
			(VOID) fcntl(0,F_SETFL,i);
			if (j < 0) {
				/*
				 * Could this have happened?
				 * I'm not sure, because the read is
				 * nonblocking. Can it be interrupted then?
				 */
				return -1;
			}
			if (j > 0) return c;
		}
	}
# else
# ifdef FIONREAD
	if (stdf >= 0) {
		/*
		 * See if there are any characters waiting in the terminal input
		 * queue. If there are not, read ahead.
		 */
		while (! ENDseen &&
		       ( ioctl(0, FIONREAD, (char *) &i) >= 0 && i == 0) &&
		       ( nopipe || fstat(stdf,&buf) >= 0 && buf.st_size > 0)) {
			/*
			 * While the user does'nt type anything, and there is
			 * input to be processed, work ahead
			 */
			if (interrupt) return -1;
			new_block();
		}
	}
# endif
# endif
	if (read(0,&c,1) <= 0) retval = -1; else retval = c & 0177;
	return retval;
}

/*
 * Get the position of line "ln" in the file.
 */

long
getpos(ln) long ln; {
	register struct block *pblock;
	register long i;

	pblock = getblock(ln,1);
	assert(pblock != 0);
	i = filldegree * (pblock - blocklist);
	return i - (filldegree - pblock->b_offs[ln - (pblock-1)->b_end]);
}

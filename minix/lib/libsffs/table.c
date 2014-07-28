/* This file contains the file system call table.
 *
 * Created:
 *   April 2009 (D.C. van Moolenbroek)
 */

#define _TABLE
#include "inc.h"

int (*call_vec[])(void) = {
	no_sys,		/*  0			*/
	no_sys,		/*  1 getnode		*/
	do_putnode,	/*  2 putnode		*/
	no_sys,		/*  3 slink		*/
	do_ftrunc,	/*  4 ftrunc		*/
	no_sys,		/*  5 chown		*/
	do_chmod,	/*  6 chmod		*/
	do_noop,	/*  7 inhibread		*/
	do_stat,	/*  8 stat		*/
	do_utime,	/*  9 utime		*/
	do_statvfs,	/* 10 statvfs		*/
	no_sys,		/* 11 bread		*/
	no_sys,		/* 12 bwrite		*/
	do_unlink,	/* 13 unlink		*/
	do_rmdir,	/* 14 rmdir		*/
	do_unmount,	/* 15 unmount		*/
	do_noop,	/* 16 sync		*/
	do_noop,	/* 17 new_driver	*/
	do_noop,	/* 18 flush		*/
	do_read,	/* 19 read		*/
	do_write,	/* 20 write		*/
	no_sys,		/* 21 mknod		*/
	do_mkdir,	/* 22 mkdir		*/
	do_create,	/* 23 create		*/
	no_sys,		/* 24 link		*/
	do_rename,	/* 25 rename		*/
	do_lookup,	/* 26 lookup		*/
	no_sys,		/* 27 mountpoint	*/
	do_readsuper,	/* 28 readsuper		*/
	no_sys,		/* 29 newnode		*/
	no_sys,		/* 30 rdlink		*/
	do_getdents,	/* 31 getdents		*/
	no_sys,		/* 32 peek		*/
	no_sys,		/* 33 bpeek		*/
};

/* This should not fail with "array size is negative": */
extern int dummy[sizeof(call_vec) == NREQS * sizeof(call_vec[0]) ? 1 : -1];

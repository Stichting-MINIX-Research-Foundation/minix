/* VTreeFS - table.c - by Alen Stojanov and David van Moolenbroek */

#define _TABLE
#include "inc.h"

int (*fs_call_vec[])(void) = {
	no_sys,		/*  0			*/
	no_sys,		/*  1	getnode		*/
	fs_putnode,	/*  2	putnode		*/
	no_sys,         /*  3	slink		*/
	no_sys,		/*  4	ftrunc		*/
	no_sys,		/*  5	chown		*/
	no_sys,		/*  6	chmod		*/
	do_noop,	/*  7	inhibread	*/
	fs_stat,	/*  8	stat		*/
	no_sys,		/*  9	utime		*/
	fs_statvfs,	/* 10	statvfs		*/
	no_sys,		/* 11	bread		*/
	no_sys,		/* 12	bwrite		*/
	no_sys,		/* 13	unlink		*/
	no_sys,		/* 14	rmdir		*/
	fs_unmount,	/* 15	unmount		*/
	do_noop,	/* 16	sync		*/
	do_noop,	/* 17	new_driver	*/
	no_sys,		/* 18	flush		*/
	fs_read,	/* 19	read		*/
	no_sys,		/* 20	write		*/
	no_sys,		/* 21	mknod		*/
	no_sys,		/* 22	mkdir		*/
	no_sys,		/* 23	create		*/
	no_sys,		/* 24	link		*/
	no_sys,		/* 25	rename		*/
	fs_lookup,	/* 26	lookup		*/
	no_sys,		/* 27	mountpoint	*/
	fs_readsuper,	/* 28	readsuper	*/
	no_sys,		/* 29	newnode		*/
	fs_rdlink,	/* 30	rdlink		*/
	fs_getdents,	/* 31	getdents	*/
	no_sys,		/* 32   peek            */
	no_sys,		/* 33   bpeek           */
};

/* This should not fail with "array size is negative": */
extern int
	dummy[sizeof(fs_call_vec) == NREQS * sizeof(fs_call_vec[0]) ? 1 : -1];

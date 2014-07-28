
/* This file contains the table used to map VFS/FS call numbers onto the
 * routines that perform them.
 */

#define _TABLE

#include "fs.h"
#include "inode.h"
#include "buf.h"

/* File System Handlers (pfs) */
int (*fs_call_vec[])(message *fs_m_in, message *fs_m_out) = {
        no_sys,             /* 0   not used */
        no_sys,             /* 1   */
        fs_putnode,         /* 2   */
        no_sys,             /* 3   */
        fs_ftrunc,          /* 4   */
        no_sys,             /* 5   */
	fs_chmod,           /* 6   */
        no_sys,             /* 7   */
        fs_stat,            /* 8   */
        no_sys,             /* 9   */
        no_sys,             /* 10  */
        no_sys,             /* 11  */
        no_sys,             /* 12  */
        no_sys,	            /* 13  */
        no_sys,             /* 14  */
        fs_unmount,         /* 15  */
	fs_sync,            /* 16  */
        no_sys,             /* 17  */
        no_sys,	            /* 18  */
        fs_readwrite,	    /* 19  */
        fs_readwrite,	    /* 20  */
        no_sys,             /* 21  */
        no_sys,             /* 22  */
        no_sys,             /* 23  */
        no_sys,             /* 24  */
        no_sys,             /* 25  */
        no_sys,             /* 26  */
        no_sys,             /* 27  */
        no_sys,	            /* 28  */
        fs_newnode,	    /* 29  */
        no_sys,	            /* 30  */
        no_sys,	            /* 31  */
	no_sys,             /* 32 */
};

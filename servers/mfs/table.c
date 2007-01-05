
/* This file contains the table used to map system call numbers onto the
 * routines that perform them.
 */

#define _TABLE

#include "fs.h"
#include <minix/callnr.h>
#include <minix/com.h>
#include "inode.h"
#include "buf.h"
#include "super.h"
#include "drivers.h"

PUBLIC _PROTOTYPE (int (*fs_call_vec[]), (void) ) = {
        no_sys,             /* 0   not used */
        fs_getnode,         /* 1   */
        fs_putnode,         /* 2   */
        fs_open,            /* 3   */
	fs_pipe,            /* 4   */
	fs_readwrite,       /* 5   */       /* read() */
	fs_readwrite,       /* 6   */       /* write() */
        fs_clone_opcl,      /* 7   */
        fs_ftrunc,          /* 8   */

        fs_chown,           /* 9   */
	fs_chmod,           /* 10  */
        fs_access,          /* 11  */
        fs_mknod,           /* 12  */
        fs_mkdir,           /* 13  */
        fs_inhibread,       /* 14  */       /* for lseek() */
        fs_stat,            /* 15  */
        fs_create,          /* 16  */
        fs_unlink,          /* 17  */       /* unlink() */
        fs_unlink,          /* 18  */       /* rmdir() */
        fs_utime,           /* 19  */
        no_sys,             /* 20  */
        fs_fstatfs,         /* 21  */
        no_sys,             /* 22  */
        no_sys,             /* 23  */
        no_sys,             /* 24  */    
        fs_link,            /* 25  */
        
        fs_slink,           /* 26  */
        fs_rdlink,          /* 27  */

        fs_rename,          /* 28  */
        no_sys,             /* 29  */
        fs_mountpoint,      /* 30  */
        fs_readsuper,       /* 31  */
        fs_unmount,         /* 32  */
        fs_trunc,           /* 33  */
	fs_sync,            /* 34  */
        lookup,             /* 35  */
	fs_stime,           /* 36  */
        fs_new_driver,      /* 37  */
        fs_breadwrite,       /* 38  */
        fs_breadwrite,       /* 39  */
        fs_getdents,	     /* 40  */
        fs_flush,	     /* 41  */
};


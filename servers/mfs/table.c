
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
        fs_slink_s,         /* 3   */
	fs_pipe,            /* 4   */
	fs_readwrite_o,     /* 5   */       /* read() */
	fs_readwrite_o,     /* 6   */       /* write() */
        fs_clone_opcl,      /* 7   */
        fs_ftrunc,          /* 8   */
        fs_chown,           /* 9   */
	fs_chmod,           /* 10  */
        fs_access_o,        /* 11  */
        fs_mknod_o,         /* 12  */
        fs_mkdir_o,         /* 13  */
        fs_inhibread,       /* 14  */       /* for lseek() */
        fs_stat,            /* 15  */
        fs_create_o,        /* 16  */
        fs_unlink_o,        /* 17  */       /* unlink() */
        fs_unlink_o,        /* 18  */       /* rmdir() */
        fs_utime,           /* 19  */
        fs_rdlink_s,        /* 20  */
        fs_fstatfs,         /* 21  */
        fs_breadwrite_s,    /* 22  */
        fs_breadwrite_s,    /* 23  */
        fs_unlink_s,	    /* 24  */    	/* unlink() */
        fs_link_o,          /* 25  */
        fs_slink_o,         /* 26  */
        fs_rdlink_o,        /* 27  */
        fs_rename_o,        /* 28  */
        fs_unlink_s,        /* 29  */		/* rmdir() */
        fs_mountpoint_o,    /* 30  */
        fs_readsuper_o,     /* 31  */
        fs_unmount,         /* 32  */
        fs_trunc,           /* 33  */
	fs_sync,            /* 34  */
        lookup_o,           /* 35  */
	fs_stime,           /* 36  */
        fs_new_driver,      /* 37  */
        fs_breadwrite_o,    /* 38  */
        fs_breadwrite_o,    /* 39  */
        fs_getdents,	    /* 40  */
        fs_flush,	    /* 41  */
        fs_readwrite_s,	    /* 42  */
        fs_readwrite_s,	    /* 43  */
        fs_mknod_s,         /* 44  */
        fs_mkdir_s,         /* 45  */
        fs_create_s,        /* 46  */
        fs_link_s,          /* 47  */
        fs_rename_s,        /* 48  */
        fs_lookup_s,        /* 49  */
        fs_mountpoint_s,    /* 50  */
        fs_readsuper_s,	    /* 51  */
        fs_newnode,	    /* 52  */
};


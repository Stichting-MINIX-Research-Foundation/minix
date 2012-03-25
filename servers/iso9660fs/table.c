
/* This file contains the table used to map system call numbers onto the
 * routines that perform them.
 */

#define _TABLE

#include "inc.h"

int (*fs_call_vec[])(void) = {
  no_sys,			/* 0: not used */
  no_sys,    			/* 1: not used */
  fs_putnode,			/* 2 */
  no_sys,			/* 3: not used */
  no_sys,			/* 4: not used */
  no_sys,			/* 5: not used */
  no_sys,			/* 6: not used */
  do_noop,			/* 7 */
  fs_stat,			/* 8 */
  no_sys,			/* 9: not used */
  fs_fstatfs,			/* 10 */
  fs_bread,			/* 11 */
  no_sys,			/* 12: not used */
  no_sys,			/* 13: not used */
  no_sys,			/* 14: not used */
  fs_unmount,			/* 15 */
  fs_sync,			/* 16 */
  fs_new_driver,		/* 17 */
  no_sys,			/* 18: not_used */
  fs_read,  			/* 19 */
  no_sys,			/* 20: not used */
  no_sys,			/* 21: not used */
  no_sys,			/* 22: not used */
  no_sys,			/* 23: not used */
  no_sys,			/* 24: not used */
  no_sys,			/* 25: not used */
  fs_lookup,			/* 26 */
  fs_mountpoint,		/* 27 */
  fs_readsuper,	                /* 28 */
  no_sys,			/* 29: not used */
  no_sys,			/* 30: not used */
  fs_getdents,			/* 31 */
  fs_statvfs,			/* 32 */
};

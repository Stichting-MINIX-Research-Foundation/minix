
/* This file contains the table used to map system call numbers onto the
 * routines that perform them.
 */

#define _TABLE

#include "inc.h"

PUBLIC _PROTOTYPE (int (*fs_call_vec[]), (void) ) = {
  no_sys,			/* 0: not used */
  no_sys,    			/* 1 */
  fs_putnode,			/* 2 */
  no_sys,			/* 3: not used */
  no_sys,			/* 8: not used */
  no_sys,			/* 9: not used */
  no_sys,			/* 10: not used */
  no_sys,			/* 14: not used */
  fs_stat,			/* 15 */
  no_sys,			/* 19: not used */
  fs_fstatfs,			/* 21 */
  fs_bread,			/* 22 */
  no_sys,			/* 23: not used */
  no_sys,			/* 24: not used */
  no_sys,			/* 29: not used */
  fs_unmount,			/* 32 */
  fs_sync,			/* 34 */
  fs_new_driver,		/* 37 */
  no_sys,			/* 41: not_used */
  fs_read,  			/* 42 */
  no_sys,			/* 43: not used */
  no_sys,			/* 44: not used */
  no_sys,			/* 45: not used */
  no_sys,			/* 46: not used */
  no_sys,			/* 47: not used */
  no_sys,			/* 48: not used */
  fs_lookup,			/* 49 */
  fs_mountpoint,		/* 50 */
  fs_readsuper,	                /* 51 */
  no_sys,			/* 52: not used */
  no_sys,			/* 53: not used */
  fs_getdents,			/* 54 */
  fs_statvfs,     /* 32 */
};

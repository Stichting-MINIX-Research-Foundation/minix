
/* This file contains the table used to map system call numbers onto the
 * routines that perform them.
 */

#define _TABLE

#include "inc.h"

PUBLIC _PROTOTYPE (int (*fs_call_vec[]), (void) ) = {
  no_sys,			/* 0: not used */
  fs_getnode,			/* 1 */
  fs_putnode,			/* 2 */
  no_sys,			/* 3: not used */
  no_sys,			/* 4: not used */
  fs_read,			/* 5 */
  no_sys,			/* 6: not used */
  no_sys,			/* 7: not used */
  no_sys,			/* 8: not used */
  no_sys,			/* 9: not used */
  no_sys,			/* 10: not used */
  fs_access,			/* 11 */
  no_sys,			/* 12: not used */
  no_sys,			/* 13: not used */
  no_sys,			/* 14: not used */
  fs_stat,			/* 15 */
  no_sys,			/* 16: not used */
  no_sys,			/* 17: not used */
  no_sys,			/* 18: not used */
  no_sys,			/* 19: not used */
  no_sys,			/* 20: not used */
  fs_fstatfs,			/* 21 */
  fs_bread_s,			/* 22 */
  no_sys,			/* 23: not used */
  no_sys,			/* 24: not used */
  no_sys,			/* 25: not used */
  no_sys,			/* 26: not used */
  no_sys,			/* 27: not used */
  no_sys,			/* 28: not used */
  no_sys,			/* 29: not used */
  no_sys,           		/* 30: not used */
  fs_readsuper,			/* 31 */
  fs_unmount,			/* 32 */
  no_sys,			/* 33: not used */
  fs_sync,			/* 34 */
  lookup,			/* 35 */
  no_sys,			/* 36: not used */
  fs_new_driver,		/* 37 */
  fs_bread,			/* 38 */
  no_sys,			/* 39 */
  fs_getdents_o,		/* 40 */
  no_sys,			/* 41: not_used */
  fs_read_s,			/* 42 */
  no_sys,			/* 43: not used */
  no_sys,			/* 44: not used */
  no_sys,			/* 45: not used */
  no_sys,			/* 46: not used */
  no_sys,			/* 47: not used */
  no_sys,			/* 48: not used */
  fs_lookup_s,			/* 49 */
  fs_mountpoint_s,		/* 50 */
  fs_readsuper_s,		/* 51 */
  no_sys,			/* 52: not used */
  no_sys,			/* 53 */
  fs_getdents,			/* 54 */
};

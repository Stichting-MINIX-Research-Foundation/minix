#ifndef FFS_FS_H
#define FFS_FS_H

/* This is the master header for the FFS (UFS2) file server.  It includes the
 * common headers and defines the principal constants.
 */
#define _SYSTEM		1	/* tell headers this is a system process */

#define VERBOSE		0	/* show messages during initialization? */

/* The following are so basic, all the *.c files get them automatically. */
#include <minix/config.h>	/* MUST be first */
#include <sys/types.h>
#include <minix/const.h>
#include <minix/type.h>

#include <lib.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>

#include <minix/syslib.h>
#include <minix/sysutil.h>

#include <minix/fsdriver.h>

#include "const.h"
#include "type.h"
#include "proto.h"
#include "glo.h"

#define ffs_debug   printf

#endif /* FFS_FS_H */

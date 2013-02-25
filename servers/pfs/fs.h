#ifndef __PFS_FS_H__
#define __PFS_FS_H__

/* This is the master header for fs.  It includes some other files
 * and defines the principal constants.
 */
#define _SYSTEM		1	/* tell headers that this is the kernel */

/* The following are so basic, all the *.c files get them automatically. */
#include <minix/config.h>	/* MUST be first */
#include <sys/types.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/dmap.h>

#include <minix/vfsif.h>
#include <limits.h>
#include <errno.h>
#include <minix/syslib.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <minix/sysutil.h>

#include "const.h"
#include "proto.h"
#include "glo.h"

#endif

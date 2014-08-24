#ifndef __MFS_FS_H__
#define __MFS_FS_H__

/* This is the master header for fs.  It includes some other files
 * and defines the principal constants.
 */
#define _SYSTEM		1	/* tell headers that this is the kernel */

#define VERBOSE		0	/* show messages during initialization? */

/* The following are so basic, all the *.c files get them automatically. */
#include <minix/config.h>	/* MUST be first */
#include <sys/types.h>
#include <minix/const.h>
#include <minix/type.h>

#include <lib.h>
#include <limits.h>
#include <errno.h>

#include <minix/syslib.h>
#include <minix/sysutil.h>

#include <minix/fsdriver.h>

#include "mfsdir.h"
#include "const.h"
#include "type.h"
#include "proto.h"
#include "glo.h"

#endif


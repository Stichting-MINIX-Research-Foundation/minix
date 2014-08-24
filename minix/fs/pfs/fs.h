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

#include <lib.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>

#include <minix/fsdriver.h>

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>

#include "const.h"
#include "proto.h"
#include "glo.h"
#include "buf.h"
#include "inode.h"

#endif

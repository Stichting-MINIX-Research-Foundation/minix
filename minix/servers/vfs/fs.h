#ifndef __VFS_FS_H__
#define __VFS_FS_H__

/* This is the master header for fs.  It includes some other files
 * and defines the principal constants.
 */
#define _SYSTEM		1	/* tell headers that this is the kernel */

/* The following are so basic, all the *.c files get them automatically. */
#include <minix/config.h>	/* MUST be first */

#include <sys/types.h>

#include <machine/vmparam.h>

#include <minix/const.h>
#include <minix/type.h>
#include <minix/dmap.h>
#include <minix/ds.h>
#include <minix/rs.h>
#include <minix/callnr.h>

#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>

#include <minix/syslib.h>
#include <minix/sysutil.h>
#include <minix/timers.h>

#include "const.h"
#include "dmap.h"
#include "proto.h"
#include "threads.h"
#include "glo.h"
#include "type.h"
#include "vmnt.h"
#include "fproc.h"

#endif

/* This is the master header for the Scheduler.  It includes some other files
 * and defines the principal constants.
 */
#define _SYSTEM		1	/* tell headers that this is the kernel */

/* The following are so basic, all the *.c files get them automatically. */
#include <minix/config.h>	/* MUST be first */
#include <sys/types.h>
#include <minix/const.h>

#include <minix/syslib.h>
#include <minix/sysutil.h>

#include <errno.h>

#include "proto.h"

extern struct machine machine;		/* machine info */

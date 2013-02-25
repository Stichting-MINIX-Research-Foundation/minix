/* This is the master header for PM.  It includes some other files
 * and defines the principal constants.
 */
#define _SYSTEM		1	/* tell headers that this is the kernel */

/* The following are so basic, all the *.c files get them automatically. */
#include <minix/config.h>	/* MUST be first */
#include <sys/types.h>
#include <minix/const.h>
#include <minix/type.h>

#include <fcntl.h>
#include <unistd.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>
#include <minix/timers.h>
#include <minix/param.h>

#include <limits.h>
#include <errno.h>
#include <sys/param.h>

#include "const.h"
#include "type.h"
#include "proto.h"
#include "glo.h"

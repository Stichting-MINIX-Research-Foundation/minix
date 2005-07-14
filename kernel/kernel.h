#ifndef KERNEL_H
#define KERNEL_H

/* This is the master header for the kernel.  It includes some other files
 * and defines the principal constants.
 */
#define _POSIX_SOURCE      1	/* tell headers to include POSIX stuff */
#define _MINIX             1	/* tell headers to include MINIX stuff */
#define _SYSTEM            1	/* tell headers that this is the kernel */

/* The following are so basic, all the *.c files get them automatically. */
#include <minix/config.h>	/* global configuration, MUST be first */
#include <ansi.h>		/* C style: ANSI or K&R, MUST be second */
#include <sys/types.h>		/* general system types */
#include <minix/const.h>	/* MINIX specific constants */
#include <minix/type.h>		/* MINIX specific types, e.g. message */
#include <minix/ipc.h>		/* MINIX run-time system */
#include <timers.h>		/* watchdog timer management */
#include <errno.h>		/* return codes and error numbers */

#if (CHIP == INTEL)
#include <ibm/portio.h>		/* device I/O and toggle interrupts */ 
#endif

/* Important kernel header files. */
#include "config.h"		/* configuration, MUST be first */
#include "const.h"		/* constants, MUST be second */
#include "type.h"		/* type definitions, MUST be third */
#include "proto.h"		/* function prototypes */
#include "glo.h"		/* global variables */
#include "ipc.h"		/* IPC constants */
#include "debug.h"		/* debugging, MUST be last kernel header */

#endif /* KERNEL_H */

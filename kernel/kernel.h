#ifndef KERNEL_H
#define KERNEL_H

/* boot verbose */
#define CONFIG_BOOT_VERBOSE

#ifndef CONFIG_MAX_CPUS
#define CONFIG_MAX_CPUS	1
#endif

/* OXPCIe952 PCIe with 2 UARTs in-kernel support */
#define CONFIG_OXPCIE	0

/* This is the master header for the kernel.  It includes some other files
 * and defines the principal constants.
 */
#define _POSIX_SOURCE      1	/* tell headers to include POSIX stuff */
#define _MINIX             1	/* tell headers to include MINIX stuff */
#define _SYSTEM            1	/* tell headers that this is the kernel */

/*
 * we need the defines above in assembly files to configure the kernel
 * correctly. However we don't need the rest
 */
#ifndef __ASSEMBLY__

/* The following are so basic, all the *.c files get them automatically. */
#include <minix/config.h>	/* global configuration, MUST be first */
#include <minix/ansi.h>		/* C style: ANSI or K&R, MUST be second */
#include <sys/types.h>		/* general system types */
#include <minix/const.h>	/* MINIX specific constants */
#include <minix/type.h>		/* MINIX specific types, e.g. message */
#include <minix/ipc.h>		/* MINIX run-time system */
#include <minix/sysutil.h>	/* MINIX utility library functions */
#include <timers.h>		/* watchdog timer management */
#include <errno.h>		/* return codes and error numbers */
#include <sys/param.h>

/* Important kernel header files. */
#include "config.h"		/* configuration, MUST be first */
#include "const.h"		/* constants, MUST be second */
#include "type.h"		/* type definitions, MUST be third */
#include "proto.h"		/* function prototypes */
#include "glo.h"		/* global variables */
#include "ipc.h"		/* IPC constants */
#include "profile.h"		/* system profiling */
#include "perf.h"		/* performance-related definitions */
#include "debug.h"		/* debugging, MUST be last kernel header */
#include "cpulocals.h"

#ifndef CONFIG_SMP
/* We only support 1 cpu now */
#define CONFIG_MAX_CPUS	1
#define cpuid		0
/* this is always true on an uniprocessor */
#define cpu_is_bsp(x) 1

#else

#include "smp.h"

#endif

#endif /* __ASSEMBLY__ */

#endif /* KERNEL_H */

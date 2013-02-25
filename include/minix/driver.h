#ifndef _MINIX_DRIVER_H
#define _MINIX_DRIVER_H

/* Types and constants shared between block and character drivers. */

#define _POSIX_SOURCE	1	/* tell headers to include POSIX stuff */
#define _NETBSD_SOURCE	1	/* tell headers to include MINIX stuff */
#define _SYSTEM		1	/* get negative error number in <errno.h> */

/* The following are so basic, all the *.c files get them automatically. */
#include <minix/config.h>	/* MUST be first */
#include <minix/type.h>
#include <minix/ipc.h>
#include <minix/com.h>
#include <minix/callnr.h>
#include <sys/types.h>
#include <minix/const.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>
#include <minix/endpoint.h>
#include <string.h>
#include <limits.h>
#include <stddef.h>
#include <errno.h>
#include <sys/param.h>
#include <minix/u64.h>
#include <minix/partition.h>

/* Base and size of a partition in bytes. */
struct device {
	u64_t dv_base;
	u64_t dv_size;
};

/* Generic receive function for all drivers. */
#ifndef driver_receive
#define driver_receive sef_receive_status
#endif

/* Maximum supported number of concurrently opened minor devices. */
#define MAX_NR_OPEN_DEVICES 256

#endif /* _MINIX_DRIVER_H */

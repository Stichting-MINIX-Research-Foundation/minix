/* This is the master header for all device drivers. It includes some other
 * files and defines the principal constants.
 */

#ifndef _INC_DRIVERS_H
#define _INC_DRIVERS_H

#define _POSIX_SOURCE	1	/* tell headers to include POSIX stuff */
#define _NETBSD_SOURCE	1	/* tell headers to include MINIX stuff */
#define _SYSTEM		1	/* get negative error number in <errno.h> */

/* The following are so basic, all the *.c files get them automatically. */
#include <minix/config.h>	/* MUST be first */
#include <minix/type.h>
#include <minix/com.h>
#include <minix/dmap.h>
#include <minix/callnr.h>
#include <sys/types.h>
#include <minix/const.h>
#include <minix/devio.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>
#include <minix/timers.h>
#include <minix/spin.h>
#include <minix/bitmap.h>

#include <machine/interrupt.h>	/* IRQ vectors and miscellaneous ports */
#if defined(__i386__)
#include <machine/bios.h>	/* BIOS index numbers */
#include <machine/ports.h>	/* Well-known ports */
#endif

#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <stddef.h>
#include <errno.h>
#include <unistd.h>
#include <sys/param.h>

#endif

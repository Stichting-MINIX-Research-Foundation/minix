/* The <lib.h> header is the master header used by the library.
 * All the C files in the lib subdirectories include it.
 */

#ifndef _LIB_H
#define _LIB_H

/* First come the defines. */
#ifdef __NBSD_LIBC
#include <sys/featuretest.h>	/* tell headers to include NetBSD stuff. */
#else /* !__NBSD_LIBC */
#define _POSIX_SOURCE      1	/* tell headers to include POSIX stuff */
#endif

#define _MINIX             1	/* tell headers to include MINIX stuff */

/* The following are so basic, all the lib files get them automatically. */
#include <minix/config.h>	/* must be first */
#include <sys/types.h>
#include <limits.h>
#include <errno.h>

#include <minix/const.h>
#include <minix/com.h>
#include <minix/type.h>
#include <minix/callnr.h>

#include <minix/ipc.h>


int __execve(const char *_path, char *const _argv[], char *const
	_envp[], int _nargs, int _nenvps);
int _syscall(endpoint_t _who, int _syscallnr, message *_msgptr);
void _loadname(const char *_name, message *_msgptr);
int _len(const char *_s);
void _begsig(int _dummy);

#endif /* _LIB_H */

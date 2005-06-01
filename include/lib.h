/* The <lib.h> header is the master header used by the library.
 * All the C files in the lib subdirectories include it.
 */

#ifndef _LIB_H
#define _LIB_H

/* First come the defines. */
#define _POSIX_SOURCE      1	/* tell headers to include POSIX stuff */
#define _MINIX             1	/* tell headers to include MINIX stuff */

/* The following are so basic, all the lib files get them automatically. */
#include <minix/config.h>	/* must be first */
#include <sys/types.h>
#include <limits.h>
#include <errno.h>
#include <ansi.h>

#include <minix/const.h>
#include <minix/com.h>
#include <minix/type.h>
#include <minix/callnr.h>

#include <minix/ipc.h>

#define MM                 PM_PROC_NR
#define FS                 FS_PROC_NR

_PROTOTYPE( int __execve, (const char *_path, char *const _argv[], 
			char *const _envp[], int _nargs, int _nenvps)	);
_PROTOTYPE( int _syscall, (int _who, int _syscallnr, message *_msgptr)	);
_PROTOTYPE( void _loadname, (const char *_name, message *_msgptr)	);
_PROTOTYPE( int _len, (const char *_s)					);
_PROTOTYPE( void _begsig, (int _dummy)					);

#endif /* _LIB_H */
